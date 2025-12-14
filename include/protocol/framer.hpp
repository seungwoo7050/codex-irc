/*
 * 설명: CRLF 기준으로 입력 버퍼를 분리하고 길이 제한을 검사한다.
 * 버전: v0.1.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/framer_test.cpp
 */
#pragma once

#include <string>
#include <vector>

namespace protocol {

struct FrameResult {
    std::vector<std::string> lines;
    bool line_too_long;
};

FrameResult ExtractLines(std::string &buffer, std::size_t max_length);

}  // namespace protocol
