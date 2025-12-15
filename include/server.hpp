/*
 * 설명: poll 기반 TCP 서버로 등록 절차, PING/PONG/QUIT, JOIN/PART, 메시징/채널 관리(TOPIC/KICK/INVITE/MODE) 라우팅과 설정 리로드를 처리한다.
 * 버전: v0.8.0
 * 관련 문서: design/protocol/contract.md, design/server/v0.7.0-modes.md, design/server/v0.8.0-config-logging.md
 * 테스트: tests/unit/framer_test.cpp, tests/unit/message_test.cpp, tests/unit/config_parser_test.cpp, tests/e2e
 */
#pragma once

#include <deque>
#include <map>
#include <poll.h>
#include <set>
#include <string>
#include <vector>

#include "protocol/framer.hpp"
#include "protocol/message.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

struct ClientConnection {
    int fd;
    std::string input_buffer;
    std::deque<std::string> outbound_queue;
    std::size_t send_offset;
    bool marked_close;
    bool pass_accepted;
    bool registered;
    bool user_set;
    std::string nick;
    std::string username;
    std::string realname;
    std::set<std::string> joined_channels;
};

struct ChannelState {
    std::set<int> members;
    std::set<int> operators;
    std::set<std::string> invited;
    std::string topic;
    bool has_topic;
    bool invite_only;
    bool topic_protected;
    bool has_key;
    std::string key;
    bool has_user_limit;
    std::size_t user_limit;

    ChannelState()
        : has_topic(false), invite_only(false), topic_protected(true), has_key(false),
          has_user_limit(false), user_limit(0) {}
};

class PollServer {
   public:
    PollServer(int port, const std::string &password, const config::Settings &settings,
               const std::string &config_path);
    void Run();

   private:
    void SetupListeningSocket();
    void EventLoop();
    void HandleListeningEvent(short revents);
    void AcceptNewClients();
    void HandleClientRead(int fd);
    void HandleClientWrite(int fd);
    void CloseClient(int fd);
    void ProcessLine(int fd, const std::string &line);
    bool EnqueueResponse(int fd, const std::string &line);
    void UpdatePollWriteInterest(int fd);
    protocol::ParsedMessage ParseAndNormalize(const std::string &line);
    void HandleCommand(int fd, const protocol::ParsedMessage &msg);
    void HandlePing(int fd, const protocol::ParsedMessage &msg);
    void HandlePong(int fd, const protocol::ParsedMessage &msg);
    void HandlePass(int fd, const protocol::ParsedMessage &msg);
    void HandleNick(int fd, const protocol::ParsedMessage &msg);
    void HandleUser(int fd, const protocol::ParsedMessage &msg);
    void HandleJoin(int fd, const protocol::ParsedMessage &msg);
    void HandlePart(int fd, const protocol::ParsedMessage &msg);
    void HandlePrivmsgNotice(int fd, const protocol::ParsedMessage &msg, bool notice);
    void HandleNames(int fd, const protocol::ParsedMessage &msg);
    void HandleList(int fd, const protocol::ParsedMessage &msg);
    void HandleTopic(int fd, const protocol::ParsedMessage &msg);
    void HandleKick(int fd, const protocol::ParsedMessage &msg);
    void HandleInvite(int fd, const protocol::ParsedMessage &msg);
    void HandleMode(int fd, const protocol::ParsedMessage &msg);
    void HandleRehash(int fd);
    void HandleQuit(int fd);
    void SendNumeric(int fd, const std::string &code, const std::string &target,
                     const std::string &message, bool close_after = false);
    bool NickInUse(const std::string &nick, int requester_fd) const;
    int FindClientFdByNick(const std::string &nick) const;
    void TryCompleteRegistration(int fd);
    void BroadcastToChannel(const std::string &channel, const std::string &line,
                            int exclude_fd = -1);
    std::string BuildUserPrefix(int fd) const;
    bool IsValidChannelName(const std::string &name) const;
    void RemoveFromAllChannels(int fd, const std::string &reason);
    void DetachClientFromChannel(int fd, const std::string &channel);
    void PromoteOperatorIfNeeded(ChannelState &state);
    bool IsChannelOperator(const ChannelState &state, int fd) const;
    bool ParsePositiveNumber(const std::string &value, std::size_t &out) const;
    std::string BuildModeReply(const ChannelState &state) const;
    void ApplyConfig(const config::Settings &settings);
    bool ReloadConfig(std::string &error);
    void HandlePendingReload();

    int listen_fd_;
    int port_;
    std::string password_;
    std::vector<struct pollfd> poll_fds_;
    std::map<int, ClientConnection> clients_;
    std::map<std::string, ChannelState> channels_;

    config::Settings config_;
    std::string config_path_;
    Logger logger_;

    std::string FormatPayloadForEcho(const std::string &payload) const;
};

