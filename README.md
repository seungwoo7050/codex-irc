# modern-irc

C++17로 작성된 단일 스레드 IRC 서버 예제다. `poll()` 기반 이벤트 루프를 사용해 다중 클라이언트를 처리하며, 버전별로 기능을 확장한다. 외부 계약은 `design/protocol/contract.md`에 기록된다.

## v0.6.0 상태
- CRLF 프레이밍과 512바이트 길이 제한을 강제하며 초과 시 연결을 종료한다.
- RFC 스타일 파서를 사용해 prefix/command/params(trailing 포함)을 분리한다.
- 지원 명령: `PASS`, `NICK`, `USER`, `PING`, `PONG`, `QUIT`, `JOIN`, `PART`, `PRIVMSG`, `NOTICE`, `NAMES`, `LIST`, `TOPIC`, `KICK`, `INVITE`
- 메시징: 등록 후 PRIVMSG/NOTICE로 사용자↔사용자 또는 채널에 메시지를 보낼 수 있으며, 채널 대상으로는 멤버에게만 브로드캐스트된다.
- NAMES/LIST: 단일 채널의 멤버 목록(353/366)과 전체 채널 목록(321/322/323)을 numeric으로 응답한다.
- PING/PONG은 payload가 없으면 `409 ERR_NOORIGIN`을 전송한다.
- 등록 완료 조건: PASS 비밀번호 일치 + NICK + USER 입력 시 `001` 환영 numeric 전송
- 등록 전 허용/거부: PASS/NICK/USER/PING/PONG/QUIT 외 명령은 `451`로 거부한다.
- 등록 이후 미지원 명령은 `421 ERR_UNKNOWNCOMMAND`로 응답한다.
- 채널 이름 규칙: `#` 시작, 2~50자, 영문/숫자/`_`/`-`만 허용.
- JOIN 성공 시 `:<nick>!<user>@modern-irc JOIN <channel>`를 채널 구성원 전체에 브로드캐스트하며, PART/QUIT/연결 종료 시에도 PART 브로드캐스트로 멤버십을 정리한다.
- 채널 관리: 첫 JOIN 사용자가 오퍼레이터가 되며, 오퍼레이터만 TOPIC 설정/INVITE/KICK을 할 수 있다. KICK은 채널 전체에 브로드캐스트되고 INVITE는 `341` 확인과 함께 초대 대상에게 전달된다. 토픽은 설정되면 TOPIC 조회 시 `332`로 응답하며 LIST의 토픽 필드에도 반영된다.
- CLI: `./modern-irc <port> <password>` (PASS 검증에 사용)

## 빌드/테스트
자세한 절차는 `CLONE_GUIDE.md`와 `verify.sh`를 참고한다.

### 빠른 명령
```bash
./verify.sh
```
