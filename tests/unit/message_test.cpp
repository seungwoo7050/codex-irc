/*
 * 설명: IRC 메시지 파서와 닉네임 검증 로직을 확인한다.
 * 버전: v0.2.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: 이 파일 자체
 */
#include "protocol/message.hpp"

#include <cassert>
#include <string>
#include <vector>

void TestParseWithPrefixAndTrailing() {
    std::string line = ":nick!user@host PRIVMSG target :hello world";
    protocol::ParsedMessage msg = protocol::ParseMessageLine(line);
    assert(msg.command == "PRIVMSG");
    assert(msg.params.size() == 2);
    assert(msg.params[0] == "target");
    assert(msg.params[1] == "hello world");
}

void TestParseWithoutPrefix() {
    std::string line = "PING token";
    protocol::ParsedMessage msg = protocol::ParseMessageLine(line);
    assert(msg.command == "PING");
    assert(msg.params.size() == 1);
    assert(msg.params[0] == "token");
}

void TestNicknameValidation() {
    assert(protocol::IsValidNickname("User1"));
    assert(protocol::IsValidNickname("nick_[]"));
    assert(!protocol::IsValidNickname(""));
    assert(!protocol::IsValidNickname("-bad"));
    assert(!protocol::IsValidNickname("bad nick"));
    assert(!protocol::IsValidNickname("bad!"));
}

int main() {
    TestParseWithPrefixAndTrailing();
    TestParseWithoutPrefix();
    TestNicknameValidation();
    return 0;
}

