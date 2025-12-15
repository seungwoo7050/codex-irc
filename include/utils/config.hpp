/*
 * 설명: INI 설정 파일을 로드해 서버 설정 구조체를 생성한다.
 * 버전: v0.9.0
 * 관련 문서: design/protocol/contract.md, design/server/v0.8.0-config-logging.md, design/server/v0.9.0-defensive.md
 * 테스트: tests/unit/config_parser_test.cpp
 */
#pragma once

#include <string>

namespace config {

enum class LogLevel { kDebug = 0, kInfo = 1, kWarn = 2, kError = 3 };

struct Settings {
    std::string server_name;
    LogLevel log_level;
    std::string log_file;
    std::size_t messages_per_5s;
    std::size_t outbound_lines;

    Settings();
};

bool LoadFromFile(const std::string &path, Settings &out, std::string &error);
std::string LogLevelToString(LogLevel level);

}  // namespace config

