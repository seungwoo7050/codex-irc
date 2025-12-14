/*
 * 설명: CRLF 프레이밍 유틸리티가 조각난 입력을 처리하는지 확인한다.
 * 버전: v0.1.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: 이 파일 자체
 */
#include <cassert>
#include <string>

#include "protocol/framer.hpp"

int main() {
    std::string buffer;
    buffer.append("PING");
    protocol::FrameResult res1 = protocol::ExtractLines(buffer, 512);
    assert(res1.lines.empty());
    assert(!res1.line_too_long);

    buffer.append(" test\r\nPONG\r\n");
    protocol::FrameResult res2 = protocol::ExtractLines(buffer, 512);
    assert(res2.lines.size() == 2);
    assert(res2.lines[0] == "PING test");
    assert(res2.lines[1] == "PONG");
    assert(buffer.empty());

    std::string long_buffer(513, 'x');
    protocol::FrameResult res3 = protocol::ExtractLines(long_buffer, 512);
    assert(res3.line_too_long);
    assert(res3.lines.empty());
    assert(long_buffer.empty());

    return 0;
}
