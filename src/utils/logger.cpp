/*
 * 설명: 로그 레벨 필터링과 파일/표준 오류 출력 제어를 담당한다.
 * 버전: v0.8.0
 * 관련 문서: design/server/v0.8.0-config-logging.md
 * 테스트: tests/e2e/test_rehash.py
 */
#include "utils/logger.hpp"

#include <iostream>
#include <sstream>

Logger::Logger() : level_(config::LogLevel::kInfo) {}

void Logger::SetLevel(config::LogLevel level) { level_ = level; }

void Logger::SetOutput(const std::string &path) {
    path_ = path;
    if (file_.is_open()) {
        file_.close();
    }

    if (!path.empty() && path != "-") {
        file_.open(path.c_str(), std::ios::out | std::ios::app);
    }
}

void Logger::Log(config::LogLevel level, const std::string &message) {
    if (!IsEnabled(level)) {
        return;
    }
    std::ostringstream oss;
    oss << "[" << config::LogLevelToString(level) << "] " << message;
    WriteLine(oss.str());
}

bool Logger::IsEnabled(config::LogLevel level) const {
    return static_cast<int>(level) >= static_cast<int>(level_);
}

void Logger::WriteLine(const std::string &line) {
    if (file_.is_open()) {
        file_ << line << '\n';
        file_.flush();
        return;
    }
    std::cerr << line << '\n';
}

