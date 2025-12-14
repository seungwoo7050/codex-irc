/*
 * 설명: CRLF 기준으로 입력 버퍼를 분리하고 길이 초과 여부를 판정한다.
 * 버전: v0.1.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/framer_test.cpp
 */
#include "protocol/framer.hpp"

#include <cstddef>

namespace protocol {

FrameResult ExtractLines(std::string &buffer, std::size_t max_length) {
    FrameResult result;
    result.line_too_long = false;

    // CRLF 도달 이전에 길이 초과한 경우 즉시 종료 플래그를 세운다.
    if (buffer.size() > max_length) {
        buffer.clear();
        result.line_too_long = true;
        return result;
    }

    std::size_t pos = std::string::npos;
    while ((pos = buffer.find("\r\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (line.size() + 2 > max_length) {
            result.line_too_long = true;
            continue;
        }
        result.lines.push_back(line);
    }

    if (buffer.size() > max_length) {
        buffer.clear();
        result.line_too_long = true;
    }

    return result;
}

}  // namespace protocol
