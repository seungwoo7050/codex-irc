/*
 * 설명: IRC 라인을 명령/파라미터로 파싱하고 닉네임을 검증한다.
 * 버전: v0.2.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/message_test.cpp
 */
#include "protocol/message.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace protocol {

ParsedMessage ParseMessageLine(const std::string &line) {
    ParsedMessage msg;

    std::size_t idx = 0;
    if (!line.empty() && line[0] == ':') {
        std::size_t space = line.find(' ');
        if (space == std::string::npos) {
            return msg;
        }
        idx = space + 1;
        while (idx < line.size() && line[idx] == ' ') {
            ++idx;
        }
    }

    std::size_t command_end = line.find(' ', idx);
    if (command_end == std::string::npos) {
        msg.command = line.substr(idx);
        return msg;
    }

    msg.command = line.substr(idx, command_end - idx);
    idx = command_end + 1;

    while (idx < line.size()) {
        if (line[idx] == ' ') {
            ++idx;
            continue;
        }
        if (line[idx] == ':') {
            msg.params.push_back(line.substr(idx + 1));
            break;
        }

        std::size_t next_space = line.find(' ', idx);
        if (next_space == std::string::npos) {
            msg.params.push_back(line.substr(idx));
            break;
        }
        msg.params.push_back(line.substr(idx, next_space - idx));
        idx = next_space + 1;
    }

    return msg;
}

bool IsValidNickname(const std::string &nick) {
    if (nick.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < nick.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(nick[i]);
        bool allowed = std::isalnum(ch) || ch == '-' || ch == '_' || ch == '[' || ch == ']' || ch == '\\';
        if (!allowed) {
            return false;
        }
        if (i == 0 && !std::isalnum(ch)) {
            return false;
        }
    }
    return true;
}

}  // namespace protocol

