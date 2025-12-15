# modern-irc

C++17로 작성된 단일 스레드 IRC 서버 예제다. `poll()` 기반 이벤트 루프를 사용해 다중 클라이언트를 처리하며, 버전별로 기능을 확장한다. 외부 계약은 `design/protocol/contract.md`에 기록된다.

## v1.0.0 상태
- CRLF 프레이밍과 512바이트 길이 제한을 강제하며 초과 시 연결을 종료한다.
- RFC 스타일 파서를 사용해 prefix/command/params(trailing 포함)을 분리한다.
- 지원 명령: `PASS`, `NICK`, `USER`, `PING`, `PONG`, `QUIT`, `JOIN`, `PART`, `PRIVMSG`, `NOTICE`, `NAMES`, `LIST`, `TOPIC`, `KICK`, `INVITE`, `MODE`, `REHASH`
- 등록 전 허용/거부: PASS/NICK/USER/PING/PONG/QUIT 외 명령은 `451`로 거부한다. PASS 오류 시 numeric 후 연결을 닫는다.
- 등록 완료 조건: PASS 비밀번호 일치 + NICK + USER 입력 시 `001` 환영 numeric 전송, 중복 닉/형식 오류/파라미터 부족 시 대응 numeric 반환.
- 채널 이름 규칙: `#` 시작, 2~50자, 영문/숫자/`_`/`-`만 허용.
- JOIN/PART/QUIT/연결 종료 시 채널에 `:<nick>!<user>@<server> JOIN/PART ...` 브로드캐스트를 보내 멤버십을 정리한다.
- 메시징: 등록 후 PRIVMSG/NOTICE로 사용자↔사용자 또는 채널에 메시지를 보낼 수 있으며, 채널 대상으로는 멤버에게만 브로드캐스트된다. 레이트리밋 초과 시 `439`로 드롭된다.
- NAMES/LIST: 단일 채널의 멤버 목록(353/366)과 전체 채널 목록(321/322/323)을 numeric으로 응답한다.
- 채널 관리: 첫 JOIN 사용자가 오퍼레이터가 되며, 오퍼레이터만 TOPIC 설정/INVITE/KICK/MODE 변경을 할 수 있다.
- 채널 모드: MODE 명령으로 +i/+t/+k/+o/+l을 적용·해제한다. +k는 키를 요구하고 +l은 인원 제한을 설정하며, +i는 초대 목록 외 사용자의 JOIN을 `473`으로 거부한다. 현재 모드는 `324`로 조회한다.
- 설정: `./modern-irc <port> <password> [config_path]`로 기동하며, INI 설정에서 서버명(`server.name`), 로그 레벨/파일(`logging.level`/`logging.file`), 레이트리밋(`limits.messages_per_5s`), 송신 큐 상한(`limits.outbound_lines`)을 지정할 수 있다.
- REHASH: 등록된 사용자가 `REHASH`를 호출하거나 프로세스가 SIGHUP을 받으면 설정 파일을 다시 읽고 서버명/로그 설정을 즉시 갱신한다. 성공 시 `382`, 실패 시 `468` numeric을 반환한다.
- 백프레셔: 각 클라이언트 송신 큐는 기본 16라인 상한을 가지며(`limits.outbound_lines`로 조정 가능), 초과 시 경고 로그를 남기고 해당 연결을 종료한다.
- 미지원: WHO/WHOIS/IRCv3 확장, TLS, 서버 링크, 사용자 모드/서비스 계정 등은 제공하지 않는다.

## 빌드/테스트
자세한 절차는 `CLONE_GUIDE.md`와 `verify.sh`를 참고한다.

### 빠른 명령
```bash
./verify.sh
```
