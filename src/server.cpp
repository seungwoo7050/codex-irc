/*
 * 설명: poll 기반 TCP 서버를 구성하고 등록 절차, PING/PONG/QUIT, JOIN/PART, 메시징과 채널 관리(TOPIC/KICK/INVITE/MODE), 설정 리로드를 처리한다.
 * 버전: v0.8.0
 * 관련 문서: design/protocol/contract.md, design/server/v0.7.0-modes.md, design/server/v0.8.0-config-logging.md
 * 테스트: tests/unit/framer_test.cpp, tests/unit/message_test.cpp, tests/unit/config_parser_test.cpp, tests/e2e
 */
#include "server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace {
const std::size_t kMaxLineLength = 512;
const std::size_t kMaxOutboundQueue = 64;
volatile std::sig_atomic_t g_reload_requested = 0;

void HandleSighup(int) { g_reload_requested = 1; }
}

PollServer::PollServer(int port, const std::string &password, const config::Settings &settings,
                       const std::string &config_path)
    : listen_fd_(-1), port_(port), password_(password), config_path_(config_path) {
    ApplyConfig(settings);
}

void PollServer::Run() {
    std::signal(SIGHUP, HandleSighup);
    SetupListeningSocket();
    EventLoop();
}

void PollServer::SetupListeningSocket() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("소켓 생성 실패");
    }

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("바인드 실패");
    }

    if (listen(listen_fd_, 16) < 0) {
        throw std::runtime_error("리스닝 실패");
    }

    struct pollfd pfd;
    pfd.fd = listen_fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;
    poll_fds_.push_back(pfd);
}

void PollServer::EventLoop() {
    while (true) {
        HandlePendingReload();

        int ret = poll(poll_fds_.data(), poll_fds_.size(), -1);
        if (ret < 0) {
            if (errno == EINTR) {
                HandlePendingReload();
                continue;
            }
            throw std::runtime_error("poll 실패");
        }

        for (std::size_t i = 0; i < poll_fds_.size(); ++i) {
            struct pollfd pfd = poll_fds_[i];
            if (pfd.revents == 0) {
                continue;
            }

            if (pfd.fd == listen_fd_) {
                HandleListeningEvent(pfd.revents);
                poll_fds_[i].revents = 0;
                continue;
            }

            if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                CloseClient(pfd.fd);
                --i;
                continue;
            }

            if (pfd.revents & POLLIN) {
                HandleClientRead(pfd.fd);
            }
            if (pfd.revents & POLLOUT) {
                HandleClientWrite(pfd.fd);
            }

            if (clients_.find(pfd.fd) == clients_.end()) {
                --i;
            } else {
                poll_fds_[i].revents = 0;
            }
        }
    }
}

void PollServer::HandleListeningEvent(short revents) {
    if (revents & POLLIN) {
        AcceptNewClients();
    }
}

void PollServer::AcceptNewClients() {
    while (true) {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr *>(&client_addr), &len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            logger_.Log(config::LogLevel::kWarn,
                        std::string("accept 실패: ") + std::strerror(errno));
            break;
        }

        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        }

        ClientConnection conn;
        conn.fd = client_fd;
        conn.send_offset = 0;
        conn.marked_close = false;
        conn.pass_accepted = false;
        conn.registered = false;
        conn.user_set = false;

        clients_[client_fd] = conn;

        struct pollfd pfd;
        pfd.fd = client_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_fds_.push_back(pfd);
    }
}

void PollServer::HandleClientRead(int fd) {
    char buf[1024];
    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            clients_[fd].input_buffer.append(buf, n);
            protocol::FrameResult res = protocol::ExtractLines(clients_[fd].input_buffer, kMaxLineLength);
            if (res.line_too_long) {
                CloseClient(fd);
                return;
            }
            for (std::size_t i = 0; i < res.lines.size(); ++i) {
                ProcessLine(fd, res.lines[i]);
                if (clients_.find(fd) == clients_.end()) {
                    return;
                }
                if (clients_[fd].marked_close) {
                    return;
                }
            }
        } else if (n == 0) {
            CloseClient(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            CloseClient(fd);
            return;
        }
    }
}

void PollServer::HandleClientWrite(int fd) {
    ClientConnection &conn = clients_[fd];
    while (!conn.outbound_queue.empty()) {
        std::string &front = conn.outbound_queue.front();
        const char *data = front.c_str() + conn.send_offset;
        std::size_t remaining = front.size() - conn.send_offset;

        ssize_t n = send(fd, data, remaining, 0);
        if (n > 0) {
            conn.send_offset += static_cast<std::size_t>(n);
            if (conn.send_offset >= front.size()) {
                conn.outbound_queue.pop_front();
                conn.send_offset = 0;
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            CloseClient(fd);
            return;
        }
    }

    UpdatePollWriteInterest(fd);

    if (conn.outbound_queue.empty() && conn.marked_close) {
        CloseClient(fd);
    }
}

void PollServer::CloseClient(int fd) {
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
        RemoveFromAllChannels(fd, "연결 종료");
        close(fd);
        clients_.erase(it);
    }

    for (std::size_t i = 0; i < poll_fds_.size(); ++i) {
        if (poll_fds_[i].fd == fd) {
            poll_fds_[i] = poll_fds_.back();
            poll_fds_.pop_back();
            break;
        }
    }
}

void PollServer::ProcessLine(int fd, const std::string &line) {
    protocol::ParsedMessage msg = ParseAndNormalize(line);
    if (msg.command.empty()) {
        return;
    }
    HandleCommand(fd, msg);
}

protocol::ParsedMessage PollServer::ParseAndNormalize(const std::string &line) {
    protocol::ParsedMessage parsed = protocol::ParseMessageLine(line);
    for (std::size_t i = 0; i < parsed.command.size(); ++i) {
        parsed.command[i] = static_cast<char>(std::toupper(parsed.command[i]));
    }
    return parsed;
}

void PollServer::HandleCommand(int fd, const protocol::ParsedMessage &msg) {
    if (msg.command == "PING") {
        HandlePing(fd, msg);
        return;
    }
    if (msg.command == "PONG") {
        HandlePong(fd, msg);
        return;
    }
    if (msg.command == "PASS") {
        HandlePass(fd, msg);
        return;
    }
    if (msg.command == "NICK") {
        HandleNick(fd, msg);
        return;
    }
    if (msg.command == "USER") {
        HandleUser(fd, msg);
        return;
    }
    if (msg.command == "JOIN") {
        HandleJoin(fd, msg);
        return;
    }
    if (msg.command == "PART") {
        HandlePart(fd, msg);
        return;
    }
    if (msg.command == "PRIVMSG") {
        HandlePrivmsgNotice(fd, msg, false);
        return;
    }
    if (msg.command == "NOTICE") {
        HandlePrivmsgNotice(fd, msg, true);
        return;
    }
    if (msg.command == "TOPIC") {
        HandleTopic(fd, msg);
        return;
    }
    if (msg.command == "KICK") {
        HandleKick(fd, msg);
        return;
    }
    if (msg.command == "INVITE") {
        HandleInvite(fd, msg);
        return;
    }
    if (msg.command == "MODE") {
        HandleMode(fd, msg);
        return;
    }
    if (msg.command == "REHASH") {
        HandleRehash(fd);
        return;
    }
    if (msg.command == "NAMES") {
        HandleNames(fd, msg);
        return;
    }
    if (msg.command == "LIST") {
        HandleList(fd, msg);
        return;
    }
    if (msg.command == "QUIT") {
        HandleQuit(fd);
        return;
    }

    if (!clients_[fd].registered) {
        SendNumeric(fd, "451", clients_[fd].nick.empty() ? "*" : clients_[fd].nick,
                    ":등록 필요");
        return;
    }

    SendNumeric(fd, "421", clients_[fd].nick, msg.command + " :알 수 없는 명령");
}

void PollServer::HandlePing(int fd, const protocol::ParsedMessage &msg) {
    if (msg.params.empty()) {
        SendNumeric(fd, "409", clients_[fd].nick.empty() ? "*" : clients_[fd].nick,
                    ":출처 없음");
        return;
    }

    std::string response = "PONG" + FormatPayloadForEcho(msg.params[0]);
    if (!EnqueueResponse(fd, response)) {
        CloseClient(fd);
    }
}

void PollServer::HandlePong(int fd, const protocol::ParsedMessage &msg) {
    if (msg.params.empty()) {
        SendNumeric(fd, "409", clients_[fd].nick.empty() ? "*" : clients_[fd].nick,
                    ":출처 없음");
        return;
    }
}

void PollServer::HandlePass(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    if (conn.registered) {
        SendNumeric(fd, "462", conn.nick.empty() ? "*" : conn.nick, ":이미 등록됨");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "461", conn.nick.empty() ? "*" : conn.nick,
                    "PASS :필수 파라미터 부족", true);
        return;
    }
    if (msg.params[0] != password_) {
        SendNumeric(fd, "464", conn.nick.empty() ? "*" : conn.nick,
                    ":비밀번호 불일치", true);
        return;
    }
    conn.pass_accepted = true;
    TryCompleteRegistration(fd);
}

void PollServer::HandleNick(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    if (conn.registered) {
        SendNumeric(fd, "462", conn.nick.empty() ? "*" : conn.nick, ":이미 등록됨");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "431", conn.nick.empty() ? "*" : conn.nick, ":닉네임 없음");
        return;
    }
    const std::string &new_nick = msg.params[0];
    if (!protocol::IsValidNickname(new_nick)) {
        SendNumeric(fd, "432", conn.nick.empty() ? "*" : conn.nick,
                    new_nick + " :닉네임 형식 오류");
        return;
    }
    if (NickInUse(new_nick, fd)) {
        SendNumeric(fd, "433", conn.nick.empty() ? "*" : conn.nick,
                    new_nick + " :닉네임 사용 중");
        return;
    }

    conn.nick = new_nick;
    TryCompleteRegistration(fd);
}

void PollServer::HandleUser(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    if (conn.registered) {
        SendNumeric(fd, "462", conn.nick.empty() ? "*" : conn.nick, ":이미 등록됨");
        return;
    }
    if (msg.params.size() < 4) {
        SendNumeric(fd, "461", conn.nick.empty() ? "*" : conn.nick,
                    "USER :필수 파라미터 부족");
        return;
    }
    conn.username = msg.params[0];
    conn.realname = msg.params[3];
    conn.user_set = true;
    TryCompleteRegistration(fd);
}

void PollServer::HandleJoin(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    if (!conn.registered) {
        SendNumeric(fd, "451", conn.nick.empty() ? "*" : conn.nick, ":등록 필요");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "461", conn.nick.empty() ? "*" : conn.nick,
                    "JOIN :필수 파라미터 부족");
        return;
    }
    const std::string &channel = msg.params[0];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", conn.nick.empty() ? "*" : conn.nick,
                    channel + " :채널 이름 오류");
        return;
    }
    if (conn.joined_channels.find(channel) != conn.joined_channels.end()) {
        SendNumeric(fd, "443", conn.nick.empty() ? "*" : conn.nick,
                    channel + " :이미 채널에 있음");
        return;
    }

    ChannelState &state = channels_[channel];
    if (!state.members.empty()) {
        if (state.invite_only && state.invited.find(conn.nick) == state.invited.end()) {
            SendNumeric(fd, "473", conn.nick.empty() ? "*" : conn.nick,
                        channel + " :초대 전용");
            return;
        }
        if (state.has_key) {
            if (msg.params.size() < 2 || msg.params[1] != state.key) {
                SendNumeric(fd, "475", conn.nick.empty() ? "*" : conn.nick,
                            channel + " :채널 키 불일치");
                return;
            }
        }
        if (state.has_user_limit && state.members.size() >= state.user_limit) {
            SendNumeric(fd, "471", conn.nick.empty() ? "*" : conn.nick,
                        channel + " :채널 인원 초과");
            return;
        }
    }
    bool was_empty = state.members.empty();
    state.members.insert(fd);
    state.invited.erase(conn.nick);
    conn.joined_channels.insert(channel);
    if (was_empty || state.operators.empty()) {
        state.operators.insert(fd);
    }

    std::string line = BuildUserPrefix(fd) + " JOIN " + channel;
    BroadcastToChannel(channel, line);
}

void PollServer::HandlePart(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    if (!conn.registered) {
        SendNumeric(fd, "451", conn.nick.empty() ? "*" : conn.nick, ":등록 필요");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "461", conn.nick.empty() ? "*" : conn.nick,
                    "PART :필수 파라미터 부족");
        return;
    }
    const std::string &channel = msg.params[0];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", conn.nick.empty() ? "*" : conn.nick,
                    channel + " :채널 이름 오류");
        return;
    }
    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it == channels_.end() || it->second.members.find(fd) == it->second.members.end()) {
        SendNumeric(fd, "442", conn.nick.empty() ? "*" : conn.nick,
                    channel + " :채널에 속해 있지 않음");
        return;
    }

    std::string reason = msg.params.size() >= 2 ? msg.params[1] : "사용자 요청";
    std::string line = BuildUserPrefix(fd) + " PART " + channel + " :" + reason;
    BroadcastToChannel(channel, line);

    DetachClientFromChannel(fd, channel);
}

void PollServer::HandlePrivmsgNotice(int fd, const protocol::ParsedMessage &msg, bool notice) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "411", nick, msg.command + " :대상 없음");
        return;
    }
    if (msg.params.size() < 2 || msg.params[1].empty()) {
        SendNumeric(fd, "412", nick, ":본문 없음");
        return;
    }

    const std::string &target = msg.params[0];
    const std::string &text = msg.params[1];
    const std::string command = notice ? " NOTICE " : " PRIVMSG ";

    if (!target.empty() && target[0] == '#') {
        if (!IsValidChannelName(target)) {
            SendNumeric(fd, "403", nick, target + " :채널 없음");
            return;
        }
        std::map<std::string, ChannelState>::iterator it = channels_.find(target);
        if (it == channels_.end()) {
            SendNumeric(fd, "403", nick, target + " :채널 없음");
            return;
        }
        if (it->second.members.find(fd) == it->second.members.end()) {
            SendNumeric(fd, "442", nick, target + " :채널에 속해 있지 않음");
            return;
        }

        std::string line = BuildUserPrefix(fd) + command + target + " :" + text;
        BroadcastToChannel(target, line, fd);
        return;
    }

    int target_fd = FindClientFdByNick(target);
    if (target_fd < 0) {
        SendNumeric(fd, "401", nick, target + " :대상 없음");
        return;
    }

    std::string line = BuildUserPrefix(fd) + command + target + " :" + text;
    if (!EnqueueResponse(target_fd, line)) {
        CloseClient(target_fd);
    }
}

void PollServer::HandleNames(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "461", nick, "NAMES :필수 파라미터 부족");
        return;
    }
    const std::string &channel = msg.params[0];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", nick, channel + " :채널 이름 오류");
        return;
    }

    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it != channels_.end() && !it->second.members.empty()) {
        std::string members_line;
        for (std::set<int>::const_iterator mem_it = it->second.members.begin();
             mem_it != it->second.members.end(); ++mem_it) {
            std::map<int, ClientConnection>::const_iterator client_it = clients_.find(*mem_it);
            if (client_it == clients_.end()) {
                continue;
            }
            if (!members_line.empty()) {
                members_line += " ";
            }
            members_line += client_it->second.nick.empty() ? "*" : client_it->second.nick;
        }
        SendNumeric(fd, "353", nick, "= " + channel + " :" + members_line);
    }

    SendNumeric(fd, "366", nick, channel + " :NAMES 종료");
}

void PollServer::HandleList(int fd, const protocol::ParsedMessage &msg) {
    (void)msg;
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }

    SendNumeric(fd, "321", nick, "Channel :Users Name");
    for (std::map<std::string, ChannelState>::const_iterator it = channels_.begin();
         it != channels_.end(); ++it) {
        const std::string count = std::to_string(it->second.members.size());
        const std::string topic = it->second.has_topic ? it->second.topic : "-";
        SendNumeric(fd, "322", nick, it->first + " " + count + " :" + topic);
    }
    SendNumeric(fd, "323", nick, ":LIST 종료");
}

void PollServer::HandleTopic(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "461", nick, "TOPIC :필수 파라미터 부족");
        return;
    }
    const std::string &channel = msg.params[0];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", nick, channel + " :채널 이름 오류");
        return;
    }
    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it == channels_.end()) {
        SendNumeric(fd, "403", nick, channel + " :채널 없음");
        return;
    }
    ChannelState &state = it->second;
    if (state.members.find(fd) == state.members.end()) {
        SendNumeric(fd, "442", nick, channel + " :채널에 속해 있지 않음");
        return;
    }

    if (msg.params.size() < 2) {
        if (!state.has_topic) {
            SendNumeric(fd, "331", nick, channel + " :토픽 없음");
        } else {
            SendNumeric(fd, "332", nick, channel + " :" + state.topic);
        }
        return;
    }

    if (state.topic_protected && !IsChannelOperator(state, fd)) {
        SendNumeric(fd, "482", nick, channel + " :채널 권한 없음");
        return;
    }

    state.topic = msg.params[1];
    state.has_topic = true;
    std::string line = BuildUserPrefix(fd) + " TOPIC " + channel + " :" + state.topic;
    BroadcastToChannel(channel, line);
}

void PollServer::HandleKick(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }
    if (msg.params.size() < 2) {
        SendNumeric(fd, "461", nick, "KICK :필수 파라미터 부족");
        return;
    }
    const std::string &channel = msg.params[0];
    const std::string &target_nick = msg.params[1];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", nick, channel + " :채널 이름 오류");
        return;
    }
    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it == channels_.end()) {
        SendNumeric(fd, "403", nick, channel + " :채널 없음");
        return;
    }
    ChannelState &state = it->second;
    if (state.members.find(fd) == state.members.end()) {
        SendNumeric(fd, "442", nick, channel + " :채널에 속해 있지 않음");
        return;
    }
    if (!IsChannelOperator(state, fd)) {
        SendNumeric(fd, "482", nick, channel + " :채널 권한 없음");
        return;
    }
    int target_fd = FindClientFdByNick(target_nick);
    if (target_fd < 0 || state.members.find(target_fd) == state.members.end()) {
        SendNumeric(fd, "441", nick, target_nick + " " + channel + " :대상이 채널에 없음");
        return;
    }

    std::string comment = msg.params.size() >= 3 ? msg.params[2] : "강퇴됨";
    std::string line = BuildUserPrefix(fd) + " KICK " + channel + " " + target_nick +
                       " :" + comment;
    BroadcastToChannel(channel, line);
    DetachClientFromChannel(target_fd, channel);
}

void PollServer::HandleInvite(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }
    if (msg.params.size() < 2) {
        SendNumeric(fd, "461", nick, "INVITE :필수 파라미터 부족");
        return;
    }
    const std::string &target_nick = msg.params[0];
    const std::string &channel = msg.params[1];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", nick, channel + " :채널 이름 오류");
        return;
    }
    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it == channels_.end()) {
        SendNumeric(fd, "403", nick, channel + " :채널 없음");
        return;
    }
    ChannelState &state = it->second;
    if (state.members.find(fd) == state.members.end()) {
        SendNumeric(fd, "442", nick, channel + " :채널에 속해 있지 않음");
        return;
    }
    if (!IsChannelOperator(state, fd)) {
        SendNumeric(fd, "482", nick, channel + " :채널 권한 없음");
        return;
    }
    int target_fd = FindClientFdByNick(target_nick);
    if (target_fd >= 0 && state.members.find(target_fd) != state.members.end()) {
        SendNumeric(fd, "443", nick, target_nick + " " + channel + " :이미 채널에 있음");
        return;
    }

    if (target_fd < 0) {
        SendNumeric(fd, "401", nick, target_nick + " :대상 없음");
        return;
    }

    state.invited.insert(target_nick);
    SendNumeric(fd, "341", nick, target_nick + " " + channel);
    std::string line = BuildUserPrefix(fd) + " INVITE " + target_nick + " " + channel;
    if (!EnqueueResponse(target_fd, line)) {
        CloseClient(target_fd);
    }
}

void PollServer::HandleMode(int fd, const protocol::ParsedMessage &msg) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }
    if (msg.params.empty()) {
        SendNumeric(fd, "461", nick, "MODE :필수 파라미터 부족");
        return;
    }
    const std::string &channel = msg.params[0];
    if (!IsValidChannelName(channel)) {
        SendNumeric(fd, "476", nick, channel + " :채널 이름 오류");
        return;
    }
    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it == channels_.end()) {
        SendNumeric(fd, "403", nick, channel + " :채널 없음");
        return;
    }
    ChannelState &state = it->second;
    if (state.members.find(fd) == state.members.end()) {
        SendNumeric(fd, "442", nick, channel + " :채널에 속해 있지 않음");
        return;
    }

    if (msg.params.size() == 1) {
        SendNumeric(fd, "324", nick, channel + " " + BuildModeReply(state));
        return;
    }

    if (!IsChannelOperator(state, fd)) {
        SendNumeric(fd, "482", nick, channel + " :채널 권한 없음");
        return;
    }

    const std::string &mode_tokens = msg.params[1];
    bool add = true;
    char last_appended_sign = '\0';
    std::string applied;
    std::vector<std::string> applied_params;
    std::size_t param_index = 2;

    for (std::size_t i = 0; i < mode_tokens.size(); ++i) {
        char c = mode_tokens[i];
        if (c == '+' || c == '-') {
            add = (c == '+');
            continue;
        }
        char sign_char = add ? '+' : '-';
        if (last_appended_sign != sign_char) {
            applied.push_back(sign_char);
            last_appended_sign = sign_char;
        }

        switch (c) {
            case 'i':
                state.invite_only = add;
                applied.push_back('i');
                break;
            case 't':
                state.topic_protected = add;
                applied.push_back('t');
                break;
            case 'k':
                if (add) {
                    if (param_index >= msg.params.size()) {
                        SendNumeric(fd, "461", nick, "MODE :필수 파라미터 부족");
                        return;
                    }
                    state.has_key = true;
                    state.key = msg.params[param_index++];
                    applied.push_back('k');
                    applied_params.push_back(state.key);
                } else {
                    state.has_key = false;
                    state.key.clear();
                    applied.push_back('k');
                }
                break;
            case 'o': {
                if (param_index >= msg.params.size()) {
                    SendNumeric(fd, "461", nick, "MODE :필수 파라미터 부족");
                    return;
                }
                const std::string &target_nick = msg.params[param_index++];
                int target_fd = FindClientFdByNick(target_nick);
                if (target_fd < 0) {
                    SendNumeric(fd, "401", nick, target_nick + " :대상 없음");
                    return;
                }
                if (state.members.find(target_fd) == state.members.end()) {
                    SendNumeric(fd, "441", nick,
                                target_nick + " " + channel + " :대상이 채널에 없음");
                    return;
                }
                if (add) {
                    state.operators.insert(target_fd);
                } else {
                    state.operators.erase(target_fd);
                    PromoteOperatorIfNeeded(state);
                }
                applied.push_back('o');
                applied_params.push_back(target_nick);
                break;
            }
            case 'l':
                if (add) {
                    if (param_index >= msg.params.size()) {
                        SendNumeric(fd, "461", nick, "MODE :필수 파라미터 부족");
                        return;
                    }
                    std::size_t limit = 0;
                    if (!ParsePositiveNumber(msg.params[param_index], limit)) {
                        SendNumeric(fd, "461", nick, "MODE :필수 파라미터 부족");
                        return;
                    }
                    ++param_index;
                    state.has_user_limit = true;
                    state.user_limit = limit;
                    applied.push_back('l');
                    applied_params.push_back(std::to_string(limit));
                } else {
                    state.has_user_limit = false;
                    state.user_limit = 0;
                    applied.push_back('l');
                }
                break;
            default:
                SendNumeric(fd, "472", nick, std::string(1, c) + " :지원하지 않는 모드");
                return;
        }
    }

    if (applied.empty()) {
        return;
    }

    std::string line = BuildUserPrefix(fd) + " MODE " + channel + " " + applied;
    for (std::size_t i = 0; i < applied_params.size(); ++i) {
        line += " " + applied_params[i];
    }
    BroadcastToChannel(channel, line);
}

void PollServer::HandleQuit(int fd) { CloseClient(fd); }

void PollServer::SendNumeric(int fd, const std::string &code, const std::string &target,
                             const std::string &message, bool close_after) {
    std::string line = std::string(":") + config_.server_name + " " + code + " " + target +
                       " " + message;
    if (!EnqueueResponse(fd, line)) {
        CloseClient(fd);
        return;
    }
    if (close_after) {
        clients_[fd].marked_close = true;
    }
}

bool PollServer::NickInUse(const std::string &nick, int requester_fd) const {
    for (std::map<int, ClientConnection>::const_iterator it = clients_.begin(); it != clients_.end();
         ++it) {
        if (it->first == requester_fd) {
            continue;
        }
        if (it->second.nick == nick) {
            return true;
        }
    }
    return false;
}

int PollServer::FindClientFdByNick(const std::string &nick) const {
    for (std::map<int, ClientConnection>::const_iterator it = clients_.begin(); it != clients_.end();
         ++it) {
        if (!it->second.registered) {
            continue;
        }
        if (it->second.nick == nick) {
            return it->first;
        }
    }
    return -1;
}

void PollServer::TryCompleteRegistration(int fd) {
    ClientConnection &conn = clients_[fd];
    if (conn.registered) {
        return;
    }
    if (!conn.pass_accepted || conn.nick.empty() || !conn.user_set) {
        return;
    }
    conn.registered = true;
    SendNumeric(fd, "001", conn.nick, ":등록 완료");
}

void PollServer::BroadcastToChannel(const std::string &channel, const std::string &line,
                                    int exclude_fd) {
    std::map<std::string, ChannelState>::iterator it = channels_.find(channel);
    if (it == channels_.end()) {
        return;
    }
    std::set<int> recipients = it->second.members;
    for (std::set<int>::iterator mem_it = recipients.begin(); mem_it != recipients.end(); ++mem_it) {
        int member_fd = *mem_it;
        if (exclude_fd >= 0 && member_fd == exclude_fd) {
            continue;
        }
        if (clients_.find(member_fd) == clients_.end()) {
            continue;
        }
        if (!EnqueueResponse(member_fd, line)) {
            CloseClient(member_fd);
        }
    }
}

std::string PollServer::BuildUserPrefix(int fd) const {
    std::map<int, ClientConnection>::const_iterator it = clients_.find(fd);
    if (it == clients_.end()) {
        return std::string(":*");
    }
    const ClientConnection &conn = it->second;
    std::string nick = conn.nick.empty() ? "*" : conn.nick;
    std::string user = conn.username.empty() ? "user" : conn.username;
    return std::string(":") + nick + "!" + user + "@" + config_.server_name;
}

bool PollServer::IsValidChannelName(const std::string &name) const {
    if (name.size() < 2 || name.size() > 50) {
        return false;
    }
    if (name[0] != '#') {
        return false;
    }
    for (std::size_t i = 1; i < name.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(c) || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

void PollServer::RemoveFromAllChannels(int fd, const std::string &reason) {
    std::map<int, ClientConnection>::iterator it = clients_.find(fd);
    if (it == clients_.end()) {
        return;
    }
    ClientConnection &conn = it->second;
    std::vector<std::string> channels(conn.joined_channels.begin(), conn.joined_channels.end());
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const std::string &channel = channels[i];
        std::map<std::string, ChannelState>::iterator chan_it = channels_.find(channel);
        if (chan_it == channels_.end()) {
            conn.joined_channels.erase(channel);
            continue;
        }
        std::string line = BuildUserPrefix(fd) + " PART " + channel + " :" + reason;
        BroadcastToChannel(channel, line);
        DetachClientFromChannel(fd, channel);
    }
}

bool PollServer::EnqueueResponse(int fd, const std::string &line) {
    ClientConnection &conn = clients_[fd];
    if (conn.outbound_queue.size() >= kMaxOutboundQueue) {
        return false;
    }
    conn.outbound_queue.push_back(line + "\r\n");
    UpdatePollWriteInterest(fd);
    return true;
}

void PollServer::UpdatePollWriteInterest(int fd) {
    for (std::size_t i = 0; i < poll_fds_.size(); ++i) {
        if (poll_fds_[i].fd == fd) {
            poll_fds_[i].events = POLLIN;
            if (!clients_[fd].outbound_queue.empty()) {
                poll_fds_[i].events |= POLLOUT;
            }
            poll_fds_[i].revents = 0;
            return;
        }
    }
}

void PollServer::DetachClientFromChannel(int fd, const std::string &channel) {
    std::map<std::string, ChannelState>::iterator chan_it = channels_.find(channel);
    if (chan_it == channels_.end()) {
        return;
    }
    ChannelState &state = chan_it->second;
    state.members.erase(fd);
    state.operators.erase(fd);

    std::map<int, ClientConnection>::iterator client_it = clients_.find(fd);
    if (client_it != clients_.end()) {
        state.invited.erase(client_it->second.nick);
        client_it->second.joined_channels.erase(channel);
    }

    if (state.members.empty()) {
        channels_.erase(chan_it);
        return;
    }
    PromoteOperatorIfNeeded(state);
}

void PollServer::PromoteOperatorIfNeeded(ChannelState &state) {
    if (state.members.empty()) {
        return;
    }
    if (!state.operators.empty()) {
        return;
    }
    int promote_fd = *state.members.begin();
    state.operators.insert(promote_fd);
}

bool PollServer::IsChannelOperator(const ChannelState &state, int fd) const {
    return state.operators.find(fd) != state.operators.end();
}

bool PollServer::ParsePositiveNumber(const std::string &value, std::size_t &out) const {
    if (value.empty()) {
        return false;
    }
    std::size_t result = 0;
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (!std::isdigit(c)) {
            return false;
        }
        result = result * 10 + static_cast<std::size_t>(c - '0');
    }
    if (result == 0) {
        return false;
    }
    out = result;
    return true;
}

std::string PollServer::BuildModeReply(const ChannelState &state) const {
    std::string modes = "+";
    if (state.invite_only) {
        modes += "i";
    }
    if (state.topic_protected) {
        modes += "t";
    }
    if (state.has_key) {
        modes += "k";
    }
    if (state.has_user_limit) {
        modes += "l";
    }

    std::string params;
    if (state.has_key) {
        params += " " + state.key;
    }
    if (state.has_user_limit) {
        params += " " + std::to_string(state.user_limit);
    }
    return modes + params;
}

void PollServer::HandleRehash(int fd) {
    ClientConnection &conn = clients_[fd];
    const std::string nick = conn.nick.empty() ? "*" : conn.nick;
    if (!conn.registered) {
        SendNumeric(fd, "451", nick, ":등록 필요");
        return;
    }

    std::string error;
    if (!ReloadConfig(error)) {
        SendNumeric(fd, "468", nick, config_path_ + " :" + error);
        return;
    }

    logger_.Log(config::LogLevel::kInfo, "REHASH 완료: " + config_path_);
    SendNumeric(fd, "382", nick, config_path_ + " :설정 리로드 완료");
}

void PollServer::ApplyConfig(const config::Settings &settings) {
    config_ = settings;
    logger_.SetLevel(config_.log_level);
    logger_.SetOutput(config_.log_file);
}

bool PollServer::ReloadConfig(std::string &error) {
    config::Settings updated;
    if (!config::LoadFromFile(config_path_, updated, error)) {
        return false;
    }
    ApplyConfig(updated);
    return true;
}

void PollServer::HandlePendingReload() {
    if (!g_reload_requested) {
        return;
    }
    g_reload_requested = 0;

    std::string error;
    if (!ReloadConfig(error)) {
        logger_.Log(config::LogLevel::kWarn, "SIGHUP 리로드 실패: " + error);
        return;
    }
    logger_.Log(config::LogLevel::kInfo,
                "SIGHUP 리로드 성공: 서버명=" + config_.server_name + " 레벨=" +
                    config::LogLevelToString(config_.log_level));
}

std::string PollServer::FormatPayloadForEcho(const std::string &payload) const {
    if (payload.empty()) {
        return "";
    }
    for (std::size_t i = 0; i < payload.size(); ++i) {
        if (payload[i] == ' ') {
            return " :" + payload;
        }
    }
    return " " + payload;
}

