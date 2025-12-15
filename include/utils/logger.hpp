/*
 * 설명: 로그 레벨과 출력 경로를 제어하는 단순 로거를 제공한다.
 * 버전: v0.8.0
 * 관련 문서: design/server/v0.8.0-config-logging.md
 * 테스트: tests/unit/config_parser_test.cpp (설정 적용 경로), tests/e2e/test_rehash.py
 */
#pragma once

#include <fstream>
#include <string>

#include "utils/config.hpp"

class Logger {
   public:
    Logger();

    void SetLevel(config::LogLevel level);
    void SetOutput(const std::string &path);
    void Log(config::LogLevel level, const std::string &message);

   private:
    config::LogLevel level_;
    std::string path_;
    std::ofstream file_;

    bool IsEnabled(config::LogLevel level) const;
    void WriteLine(const std::string &line);
};

