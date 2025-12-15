/*
 * 설명: IRC 라인을 RFC 규칙에 따라 prefix/command/params로 파싱하고 닉네임 유효성을 검사한다.
 * 버전: v0.3.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/message_test.cpp
 */
#pragma once

#include <string>
#include <vector>

namespace protocol {

struct ParsedMessage {
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
};

ParsedMessage ParseMessageLine(const std::string &line);
bool IsValidNickname(const std::string &nick);

}  // namespace protocol

