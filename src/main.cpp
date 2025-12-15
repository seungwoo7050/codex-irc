/*
 * 설명: modern-irc 실행 진입점으로 서버를 초기화하고 설정 파일을 반영한다.
 * 버전: v0.8.0
 * 관련 문서: design/protocol/contract.md, design/server/v0.8.0-config-logging.md
 * 테스트: tests/e2e
 */
#include <cstdlib>
#include <iostream>
#include <string>

#include "server.hpp"
#include "utils/config.hpp"

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        std::cerr << "사용법: ./modern-irc <port> <password> [config_path]\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    std::string password = argv[2];
    std::string config_path = argc == 4 ? argv[3] : "config/server.ini";

    config::Settings settings;
    std::string error;
    if (!config::LoadFromFile(config_path, settings, error)) {
        std::cerr << "설정 파일 오류: " << error << "\n";
        return 1;
    }

    try {
        PollServer server(port, password, settings, config_path);
        server.Run();
    } catch (const std::exception &ex) {
        std::cerr << "서버 오류: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
