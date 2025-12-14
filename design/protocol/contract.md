# design/protocol/contract.md

## 개요 (v0.2.0)
- 본 문서는 modern-irc 서버의 외부 프로토콜 계약을 정의한다.
- v0.2.0은 등록 절차(PASS/NICK/USER)와 등록 전 커맨드 허용/거부 정책을 고정한다.

---

## 연결 및 CLI
- 실행 방법: `./modern-irc <port> <password>`
  - `<port>`: IPv4 TCP 포트 번호.
  - `<password>`: 서버 공유 비밀번호. v0.2.0에서는 PASS 명령 검증에 사용한다.
- 서버는 IPv4에서 `INADDR_ANY`로 바인드하며, `poll()` 기반 단일 스레드 이벤트 루프로 동작한다.

---

## 입력 프레이밍 정책
- 메시지 구분자는 CRLF(`\r\n`)이다.
- 각 클라이언트는 개별 입력 버퍼를 가진다.
  - 부분 수신을 허용하며, CRLF가 도달할 때까지 버퍼링한다.
- 라인 최대 길이: **512바이트(종료 CRLF 포함)**
  - CRLF를 찾았을 때, 해당 라인이 512바이트를 초과하면 즉시 연결을 종료한다.
  - CRLF가 오지 않은 상태에서 버퍼가 512바이트를 넘으면 버퍼를 비우고 연결을 종료한다.

---

## 출력 큐(백프레셔) 정책
- 각 클라이언트는 송신 대기열(deque<string>)을 가진다.
- 상한: **64개 라인**.
- 새 라인을 추가하려 할 때 큐가 가득 차 있으면, 해당 클라이언트를 로그만 남기고 종료한다(추가 드롭 없이 종료).

---

## 등록 흐름 (v0.2.0)
- 필수 입력: PASS(정확한 비밀번호), NICK(유효한 닉네임), USER(사용자명, realname).
- 등록 완료 조건: **PASS 성공 + NICK 설정 + USER 설정**이 모두 충족되는 즉시 "registered" 상태로 전환하고 환영 numeric을 전송한다.
- 허용 순서: PASS/NICK/USER는 어느 순서든 가능하지만, PASS는 등록 완료 전에만 허용된다.
- 닉네임 제약: 영문/숫자/`[]\\`_`-` 만 허용하며, 첫 글자는 영문 또는 숫자여야 한다.
- 중복 닉네임: 동일 닉네임이 이미 등록된 클라이언트가 있으면 거부한다.

### 등록 전 허용/거부 커맨드
- 허용: PASS, NICK, USER, PING, QUIT
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

## 지원 명령 (v0.2.0)
- PING
  - 요청 형식: `PING <payload>`
  - 응답 형식: `PONG <payload>`\r\n
  - `<payload>`는 공백 없는 토큰을 기대하며, 비어 있으면 빈 문자열 그대로 반사한다.
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

## 오류 처리
- 라인 길이 초과: 연결 종료.
- 송신 큐 초과: 연결 종료.
- 소켓 오류 또는 EOF: 연결 종료.
- 등록 실패:
  - PASS 비밀번호 불일치 또는 파라미터 부족: numeric 전송 후 연결 종료.
  - 이미 등록된 상태에서 PASS/NICK/USER: numeric만 전송하고 연결은 유지한다.
  - NICK 형식 오류/중복: numeric 전송 후 연결 유지(재시도 허용).
- 응답 포맷: 모든 응답 라인은 CRLF로 종료된다.

---

## CRLF 보장
- 서버가 보내는 모든 응답 라인은 CRLF(`\r\n`)로 종료된다.
