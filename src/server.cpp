/*
 * 설명: poll 기반 TCP 서버를 구성하고 PING 요청에 PONG을 응답한다.
 * 버전: v0.1.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/framer_test.cpp, tests/e2e/smoke_test.py
 */
#include "server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
const std::size_t kMaxLineLength = 512;
const std::size_t kMaxOutboundQueue = 64;
}

PollServer::PollServer(int port, const std::string &password)
    : listen_fd_(-1), port_(port), password_(password) {}

void PollServer::Run() {
    static_cast<void>(password_);
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
        int ret = poll(poll_fds_.data(), poll_fds_.size(), -1);
        if (ret < 0) {
            if (errno == EINTR) {
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
            std::cerr << "accept 실패: " << std::strerror(errno) << "\n";
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
}

void PollServer::CloseClient(int fd) {
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
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
    std::string command = line;
    std::string payload;

    std::size_t space = line.find(' ');
    if (space != std::string::npos) {
        command = line.substr(0, space);
        payload = line.substr(space + 1);
    }

    for (std::size_t i = 0; i < command.size(); ++i) {
        command[i] = static_cast<char>(std::toupper(command[i]));
    }

    if (command == "PING") {
        std::string response = "PONG";
        if (!payload.empty()) {
            response += " " + payload;
        }
        if (!EnqueueResponse(fd, response)) {
            CloseClient(fd);
        }
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

