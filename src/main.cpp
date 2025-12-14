/*
 * 설명: modern-irc 실행 진입점으로 서버를 초기화한다.
 * 버전: v0.2.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/e2e
 */
#include <cstdlib>
#include <iostream>
#include <string>

#include "server.hpp"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "사용법: ./modern-irc <port> <password>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    std::string password = argv[2];

    try {
        PollServer server(port, password);
        server.Run();
    } catch (const std::exception &ex) {
        std::cerr << "서버 오류: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
