/*
 * 설명: poll 기반 TCP 서버로 등록 절차와 PING/PONG/QUIT 및 에러 응답을 처리한다.
 * 버전: v0.3.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/framer_test.cpp, tests/unit/message_test.cpp, tests/e2e
 */
#pragma once

#include <deque>
#include <map>
#include <poll.h>
#include <string>
#include <vector>

#include "protocol/framer.hpp"
#include "protocol/message.hpp"

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
    protocol::ParsedMessage ParseAndNormalize(const std::string &line);
    void HandleCommand(int fd, const protocol::ParsedMessage &msg);
    void HandlePing(int fd, const protocol::ParsedMessage &msg);
    void HandlePong(int fd, const protocol::ParsedMessage &msg);
    void HandlePass(int fd, const protocol::ParsedMessage &msg);
    void HandleNick(int fd, const protocol::ParsedMessage &msg);
    void HandleUser(int fd, const protocol::ParsedMessage &msg);
    void HandleQuit(int fd);
    void SendNumeric(int fd, const std::string &code, const std::string &target,
                     const std::string &message, bool close_after = false);
    bool NickInUse(const std::string &nick, int requester_fd) const;
    void TryCompleteRegistration(int fd);

    int listen_fd_;
    int port_;
    std::string password_;
    std::vector<struct pollfd> poll_fds_;
    std::map<int, ClientConnection> clients_;

    std::string FormatPayloadForEcho(const std::string &payload) const;
};

