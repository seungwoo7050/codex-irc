/*
 * 설명: poll 기반 TCP 서버로 PING 요청을 처리한다.
 * 버전: v0.1.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/framer_test.cpp, tests/e2e/smoke_test.py
 */
#pragma once

#include <deque>
#include <map>
#include <poll.h>
#include <string>
#include <vector>

#include "protocol/framer.hpp"

struct ClientConnection {
    int fd;
    std::string input_buffer;
    std::deque<std::string> outbound_queue;
    std::size_t send_offset;
    bool marked_close;
};

class PollServer {
   public:
    PollServer(int port, const std::string &password);
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

    int listen_fd_;
    int port_;
    std::string password_;
    std::vector<struct pollfd> poll_fds_;
    std::map<int, ClientConnection> clients_;
};

