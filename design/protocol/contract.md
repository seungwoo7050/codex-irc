# design/protocol/contract.md

## 개요 (v0.4.0)
- 본 문서는 modern-irc 서버의 외부 프로토콜 계약을 정의한다.
- v0.4.0은 RFC 스타일 메시지 파싱 규칙, PING/PONG/QUIT 동작, 채널 JOIN/PART 흐름과 브로드캐스트 메시지를 고정한다.

---

## 연결 및 CLI
- 실행 방법: `./modern-irc <port> <password>`
  - `<port>`: IPv4 TCP 포트 번호.
  - `<password>`: 서버 공유 비밀번호. PASS 명령 검증에 사용한다.
- 서버는 IPv4에서 `INADDR_ANY`로 바인드하며, `poll()` 기반 단일 스레드 이벤트 루프로 동작한다.

---

## 입력 프레이밍 및 메시지 문법
- 메시지 구분자는 CRLF(`\r\n`)이다.
- 각 클라이언트는 개별 입력 버퍼를 가진다.
  - 부분 수신을 허용하며, CRLF가 도달할 때까지 버퍼링한다.
- 라인 최대 길이: **512바이트(종료 CRLF 포함)**
  - CRLF를 찾았을 때 해당 라인이 512바이트를 초과하면 **즉시 연결을 종료**한다(에러 라인 전송 없음).
  - CRLF가 오지 않은 상태에서 버퍼가 512바이트를 넘으면 버퍼를 비우고 연결을 종료한다.
- 메시지 파싱 규칙:
  - 선택적 prefix: 라인이 `:`로 시작하면 prefix는 다음 공백 전까지이며, 이후 공백들은 모두 스킵한다.
  - command: prefix 이후 첫 토큰. 서버 내부에서는 대문자로 정규화하여 매칭한다.
  - params: command 뒤 공백으로 구분된 토큰들.
    - 토큰이 `:`로 시작하면 해당 위치부터 라인 끝까지를 **단일 trailing 파라미터**로 취급하고 더 이상 분리하지 않는다.
    - 앞뒤의 연속된 공백은 모두 무시하며, 빈 파라미터는 생성하지 않는다.

---

## 출력 큐(백프레셔) 정책
- 각 클라이언트는 송신 대기열(deque<string>)을 가진다.
- 상한: **64개 라인**.
- 새 라인을 추가하려 할 때 큐가 가득 차 있으면, 해당 클라이언트를 로그만 남기고 종료한다(추가 드롭 없이 종료).

---

## 등록 흐름 (v0.3.0)
- 필수 입력: PASS(정확한 비밀번호), NICK(유효한 닉네임), USER(사용자명, realname).
- 등록 완료 조건: **PASS 성공 + NICK 설정 + USER 설정**이 모두 충족되는 즉시 "registered" 상태로 전환하고 환영 numeric을 전송한다.
- 허용 순서: PASS/NICK/USER는 어느 순서든 가능하지만, PASS는 등록 완료 전에만 허용된다.
- 닉네임 제약: 영문/숫자/`[]\\`_`-` 만 허용하며, 첫 글자는 영문 또는 숫자여야 한다.
- 중복 닉네임: 동일 닉네임이 이미 등록된 클라이언트가 있으면 거부한다.

### 등록 전 허용/거부 커맨드
- 허용: PASS, NICK, USER, PING, PONG, QUIT
- 거부: 그 외 모든 커맨드(JOIN, PRIVMSG 등)는 `ERR_NOTREGISTERED`로 거부한다.

### 등록 성공/실패 numeric
- 성공: `001 RPL_WELCOME` – 형식 `:modern-irc 001 <nick> :등록 완료`.
- PASS 오류:
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS PASS :필수 파라미터 부족`
  - 비밀번호 불일치: `464 ERR_PASSWDMISMATCH :비밀번호 불일치`
  - 이미 등록: `462 ERR_ALREADYREGISTRED :이미 등록됨`
- NICK 오류:
  - 파라미터 없음: `431 ERR_NONICKNAMEGIVEN :닉네임 없음`
  - 형식 오류: `432 ERR_ERRONEUSNICKNAME <nick> :닉네임 형식 오류`
  - 중복: `433 ERR_NICKNAMEINUSE <nick> :닉네임 사용 중`
  - 이미 등록 후 재시도: `462 ERR_ALREADYREGISTRED :이미 등록됨`
- USER 오류:
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS USER :필수 파라미터 부족`
  - 이미 등록: `462 ERR_ALREADYREGISTRED :이미 등록됨`
- 등록 전 거부: `451 ERR_NOTREGISTERED :등록 필요`

---

## 지원 명령 (v0.3.0)
- PING
  - 요청 형식: `PING <payload>`
  - 응답 형식: `PONG <payload>`\r\n
  - `<payload>`가 없으면 `409 ERR_NOORIGIN :출처 없음`을 전송한다.
- PONG
  - 요청 형식: `PONG <payload>`
  - 동작: 서버는 유효성을 확인한 뒤 별도 응답 없이 연결을 유지한다. `<payload>`가 없으면 `409 ERR_NOORIGIN :출처 없음`을 전송한다.
- QUIT
  - 요청 형식: `QUIT [:<message>]`
  - 동작: 입력 여부와 무관하게 연결을 종료한다. 별도 브로드캐스트는 없다.
- PASS
  - 요청 형식: `PASS <password>`
  - 효과: 등록 완료 전 비밀번호 검증. 정확히 일치해야 하며, 성공 시 이후 등록 완료 조건에서 PASS 완료로 취급한다.
- NICK
  - 요청 형식: `NICK <nickname>`
  - 효과: 닉네임 설정. 유효성/중복 검사 후 성공 시 등록 상태 갱신.
- USER
  - 요청 형식: `USER <username> 0 * :<realname>`
  - 효과: username과 realname을 설정한다. 모드/호스트 필드는 무시한다.

---

## 오류 처리 및 numeric replies (v0.3.0)
- 길이 초과 정책: **라인 길이 512바이트 초과 시 즉시 연결 종료**(드롭/에러 미전송).
- 송신 큐 초과: 연결 종료.
- 소켓 오류 또는 EOF: 연결 종료.
- 등록 실패:
  - PASS 비밀번호 불일치 또는 파라미터 부족: numeric 전송 후 연결 종료.
  - 이미 등록된 상태에서 PASS/NICK/USER: numeric만 전송하고 연결은 유지한다.
  - NICK 형식 오류/중복: numeric 전송 후 연결 유지(재시도 허용).
- 명령별 파라미터 부족: `461 ERR_NEEDMOREPARAMS <command> :필수 파라미터 부족`.
- PING/PONG 파라미터 없음: `409 ERR_NOORIGIN :출처 없음`.
- 등록 전 거부: `451 ERR_NOTREGISTERED :등록 필요`.
- 알 수 없는 명령: `421 ERR_UNKNOWNCOMMAND <command> :알 수 없는 명령`.
- 응답 포맷: 모든 응답 라인은 CRLF로 종료된다.

### v0.3.0에서 사용하는 numeric 목록
- 001 RPL_WELCOME – 등록 성공 시 `:modern-irc 001 <nick> :등록 완료`
- 409 ERR_NOORIGIN – PING/PONG 파라미터 부족
- 421 ERR_UNKNOWNCOMMAND – 등록 후 미지원 명령
- 431 ERR_NONICKNAMEGIVEN – NICK 파라미터 없음
- 432 ERR_ERRONEUSNICKNAME – 닉네임 형식 오류
- 433 ERR_NICKNAMEINUSE – 닉네임 중복
- 451 ERR_NOTREGISTERED – 등록 전 제한
- 461 ERR_NEEDMOREPARAMS – 필수 파라미터 부족
- 462 ERR_ALREADYREGISTRED – 이미 등록된 상태에서 PASS/NICK/USER 수행
- 464 ERR_PASSWDMISMATCH – PASS 비밀번호 불일치

---

## 채널 규칙 및 JOIN/PART (v0.4.0)
- **채널 이름 규칙**
  - `#`로 시작해야 한다.
  - 길이: 2~50자.
  - 허용 문자: 영문 대소문자, 숫자, `_` `-` (공백/쉼표 없음).
  - 대소문자를 구분한다.

- **공통 정책**
  - JOIN/PART는 등록 완료 후에만 허용된다. 등록 전에는 `451 ERR_NOTREGISTERED`로 거부한다.
  - 한 번에 하나의 채널만 처리한다(쉼표 분리 목록 미지원).
  - 서버는 채널 멤버십을 추적하며, 퇴장하거나 연결이 종료되면 해당 채널 멤버십을 제거한다.

- **JOIN**
  - 문법: `JOIN <channel>`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS JOIN :필수 파라미터 부족`
  - 채널 이름 형식 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 이미 채널에 있는 경우: `443 ERR_USERONCHANNEL <nick> <channel> :이미 채널에 있음`
  - 성공 시: 채널이 없으면 생성하고 멤버십을 추가한 뒤 **채널 전체**(자신 포함)에 `:<nick>!<username>@modern-irc JOIN <channel>` 를 브로드캐스트한다.

- **PART**
  - 문법: `PART <channel> [:<message>]`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS PART :필수 파라미터 부족`
  - 채널 이름 형식 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 채널이 없거나 멤버가 아닌 경우: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
  - 성공 시: 채널 구성원 전체(자신 포함)에 `:<nick>!<username>@modern-irc PART <channel> :<message>` 를 브로드캐스트한다.
    - `<message>`가 없으면 `:사용자 요청`으로 대체한다.
    - 브로드캐스트 후 멤버십을 제거하며, 채널이 비면 삭제한다.

- **연결 종료/QUIT 시 멤버십 정리**
  - QUIT 또는 소켓 종료 시, 사용자가 속했던 각 채널에 `:<nick>!<username>@modern-irc PART <channel> :연결 종료`를 브로드캐스트한 뒤 멤버십을 제거한다.

### v0.4.0에서 사용하는 numeric 추가 목록
- 442 ERR_NOTONCHANNEL – PART 시 채널 미가입
- 443 ERR_USERONCHANNEL – JOIN 시 이미 가입
- 476 ERR_BADCHANMASK – 채널 이름 규칙 위반

---

## CRLF 보장
- 서버가 보내는 모든 응답 라인은 CRLF(`\r\n`)로 종료된다.
