/*
 * 설명: INI 설정 파서가 기본값과 사용자 지정 값을 올바르게 해석하는지 확인한다.
 * 버전: v0.9.0
 * 관련 문서: design/protocol/contract.md, design/server/v0.8.0-config-logging.md, design/server/v0.9.0-defensive.md
 * 테스트: 이 파일 자체
 */
#include "utils/config.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

void TestDefaultsWhenFileMissing() {
    config::Settings settings;
    std::string error;
    bool ok = config::LoadFromFile("tests/unit/does_not_exist.ini", settings, error);
    assert(ok);
    assert(error.empty());
    assert(settings.server_name == "modern-irc");
    assert(settings.log_level == config::LogLevel::kInfo);
    assert(settings.log_file.empty());
    assert(settings.messages_per_5s == 0);
    assert(settings.outbound_lines == 16);
}

void TestParseCustomValues() {
    const std::string path = "tests/unit/sample_config.ini";
    std::ofstream file(path.c_str());
    file << "[server]\n";
    file << "name=custom-irc\n";
    file << "[logging]\n";
    file << "level=warn\n";
    file << "file=logs/server.log\n";
    file << "[limits]\n";
    file << "messages_per_5s=15\n";
    file << "outbound_lines=10\n";
    file.close();

    config::Settings settings;
    std::string error;
    bool ok = config::LoadFromFile(path, settings, error);
    assert(ok);
    assert(error.empty());
    assert(settings.server_name == "custom-irc");
    assert(settings.log_level == config::LogLevel::kWarn);
    assert(settings.log_file == "logs/server.log");
    assert(settings.messages_per_5s == 15);
    assert(settings.outbound_lines == 10);

    std::remove(path.c_str());
}

void TestRejectInvalid() {
    const std::string path = "tests/unit/bad_config.ini";
    std::ofstream file(path.c_str());
    file << "[logging]\n";
    file << "level=verbose\n";
    file.close();

    config::Settings settings;
    std::string error;
    bool ok = config::LoadFromFile(path, settings, error);
    assert(!ok);
    assert(!error.empty());

    std::remove(path.c_str());
}

int main() {
    TestDefaultsWhenFileMissing();
    TestParseCustomValues();
    TestRejectInvalid();
    return 0;
}

