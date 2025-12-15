/*
 * 설명: INI 파일을 파싱해 서버 설정을 생성하고 검증한다.
 * 버전: v0.9.0
 * 관련 문서: design/protocol/contract.md, design/server/v0.8.0-config-logging.md, design/server/v0.9.0-defensive.md
 * 테스트: tests/unit/config_parser_test.cpp
 */
#include "utils/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
bool StartsWith(const std::string &text, char c) { return !text.empty() && text[0] == c; }

std::string Trim(const std::string &text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(start, end - start);
}

std::string ToLower(const std::string &text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

bool ParseLogLevel(const std::string &raw, config::LogLevel &out) {
    const std::string lowered = ToLower(raw);
    if (lowered == "debug") {
        out = config::LogLevel::kDebug;
        return true;
    }
    if (lowered == "info") {
        out = config::LogLevel::kInfo;
        return true;
    }
    if (lowered == "warn") {
        out = config::LogLevel::kWarn;
        return true;
    }
    if (lowered == "error") {
        out = config::LogLevel::kError;
        return true;
    }
    return false;
}

bool ParsePositiveNumber(const std::string &raw, std::size_t &out) {
    if (raw.empty()) {
        return false;
    }
    char *end = NULL;
    unsigned long value = std::strtoul(raw.c_str(), &end, 10);
    if (end == NULL || *end != '\0') {
        return false;
    }
    out = static_cast<std::size_t>(value);
    return true;
}
}  // namespace

namespace config {

Settings::Settings()
    : server_name("modern-irc"), log_level(LogLevel::kInfo), messages_per_5s(0), outbound_lines(16) {}

bool LoadFromFile(const std::string &path, Settings &out, std::string &error) {
    Settings defaults;
    defaults.log_file = "";
    out = defaults;

    if (path.empty()) {
        return true;
    }

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return true;
    }

    std::string section;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(file, line)) {
        ++line_no;
        std::string trimmed = Trim(line);
        if (trimmed.empty() || StartsWith(trimmed, '#') || StartsWith(trimmed, ';')) {
            continue;
        }

        if (StartsWith(trimmed, '[')) {
            if (trimmed.size() < 3 || trimmed.back() != ']') {
                std::ostringstream oss;
                oss << "잘못된 섹션 선언 (" << line_no << ")";
                error = oss.str();
                return false;
            }
            section = ToLower(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        std::size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            std::ostringstream oss;
            oss << "키=값 형식 오류 (" << line_no << ")";
            error = oss.str();
            return false;
        }

        std::string key = ToLower(Trim(trimmed.substr(0, eq_pos)));
        std::string value = Trim(trimmed.substr(eq_pos + 1));
        if (section.empty()) {
            std::ostringstream oss;
            oss << "섹션 없음 (" << line_no << ")";
            error = oss.str();
            return false;
        }

        if (section == "server" && key == "name") {
            if (value.empty()) {
                std::ostringstream oss;
                oss << "server.name 누락 (" << line_no << ")";
                error = oss.str();
                return false;
            }
            out.server_name = value;
        } else if (section == "logging" && key == "level") {
            LogLevel parsed;
            if (!ParseLogLevel(value, parsed)) {
                std::ostringstream oss;
                oss << "logging.level 오류 (" << line_no << ")";
                error = oss.str();
                return false;
            }
            out.log_level = parsed;
        } else if (section == "logging" && key == "file") {
            out.log_file = value;
        } else if (section == "limits" && key == "messages_per_5s") {
            std::size_t number = 0;
            if (!ParsePositiveNumber(value, number)) {
                std::ostringstream oss;
                oss << "limits.messages_per_5s 오류 (" << line_no << ")";
                error = oss.str();
                return false;
            }
            out.messages_per_5s = number;
        } else if (section == "limits" && key == "outbound_lines") {
            std::size_t number = 0;
            if (!ParsePositiveNumber(value, number)) {
                std::ostringstream oss;
                oss << "limits.outbound_lines 오류 (" << line_no << ")";
                error = oss.str();
                return false;
            }
            out.outbound_lines = number;
        } else {
            std::ostringstream oss;
            oss << "알 수 없는 섹션/키 (" << line_no << ")";
            error = oss.str();
            return false;
        }
    }

    return true;
}

std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug:
            return "debug";
        case LogLevel::kInfo:
            return "info";
        case LogLevel::kWarn:
            return "warn";
        case LogLevel::kError:
            return "error";
    }
    return "info";
}

}  // namespace config

