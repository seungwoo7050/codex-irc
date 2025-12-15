# design/protocol/contract.md

## 개요 (v0.8.0)
- 본 문서는 modern-irc 서버의 외부 프로토콜 계약을 정의한다.
- v0.8.0에서는 INI 설정 파일로 서버명을 포함한 운영 파라미터를 지정하고, REHASH/SIGHUP으로 런타임 리로드 규칙을 고정한다.

---

## 연결 및 CLI
- 실행 방법: `./modern-irc <port> <password> [config_path]`
  - `<port>`: IPv4 TCP 포트 번호.
  - `<password>`: 서버 공유 비밀번호. PASS 명령 검증에 사용한다.
  - `[config_path]`: 선택적 INI 설정 파일 경로. 생략 시 `config/server.ini`를 사용하며, 파일이 없으면 기본값으로 기동한다.
- 서버는 IPv4에서 `INADDR_ANY`로 바인드하며, `poll()` 기반 단일 스레드 이벤트 루프로 동작한다.

### 설정 파일 (INI)
- 섹션/키는 소문자로 고정하며, 공백을 포함하지 않는 `키=값` 형식을 따른다.
- `#` 또는 `;`로 시작하는 줄은 주석으로 무시한다.
- 지원 섹션 및 키(기본값 포함):
  - `[server]`
    - `name` (기본: `modern-irc`)
      - numeric prefix와 사용자 prefix의 호스트 부분에 사용된다.
  - `[logging]`
    - `level` (기본: `info`, 허용: `debug|info|warn|error`, 대소문자 무시)
    - `file` (기본: 빈 문자열 → 표준 오류로 출력, `-`도 표준 오류 의미)
  - `[limits]`
    - `messages_per_5s` (기본: `0`, 0이면 비활성화) — v0.8.0에서는 **값만 로드**하며, 차후 버전(v0.9.0)에서 실제 레이트리밋에 사용한다.
- 설정 파일이 없으면 모든 키가 기본값으로 채워진다.
- 파일이 존재하지만 구문/값이 잘못되면 로드에 실패하며, 실패 시 이전 구성이 유지된다.

### 로깅 규칙
- 로그 레벨: debug < info < warn < error 순서로 필터링한다.
- 출력 대상: `logging.file`이 비어 있거나 `-`이면 표준 오류로 기록하며, 경로가 주어지면 append 모드로 파일을 연다.
- REHASH/SIGHUP으로 로그 레벨·출력 경로가 바뀌면 즉시 새 설정이 적용된다.

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

### 설정 리로드 (v0.8.0)
- **REHASH**
  - 요청 형식: `REHASH`
  - 등록 전 호출: `451 ERR_NOTREGISTERED :등록 필요`
  - 동작: 서버는 기동 시점에 사용한 설정 파일 경로를 다시 읽어 들이고, **성공 시 즉시 새 설정을 적용**한다.
  - 성공 응답: `382 RPL_REHASHING <path> :설정 리로드 완료`
    - prefix로 사용되는 서버명은 적용된 새 설정(`server.name`)을 따른다.
  - 실패 응답: `468 ERR_REHASHFAILED <path> :<사유>` (잘못된 키/값 등)
- **SIGHUP**
  - 프로세스가 SIGHUP을 받으면 REHASH와 동일하게 설정을 다시 읽는다.
  - SIGHUP은 클라이언트 응답 없이 로그만 남긴다.

---

## 메시징 명령 (v0.5.0)
- 공통 정책
  - 등록이 완료되지 않은 클라이언트가 PRIVMSG/NOTICE/NAMES/LIST를 호출하면 `451 ERR_NOTREGISTERED :등록 필요`로 거부한다.
  - 대상이 채널인 경우 채널 이름 규칙은 JOIN과 동일하게 적용한다.

- PRIVMSG
  - 문법: `PRIVMSG <target> :<text>`
  - `<target>`: 닉네임 또는 채널 이름.
  - `<text>`: 공백 포함 메시지 본문. trailing 파라미터(`:<text>`)를 사용한다.
  - 오류:
    - 대상 미지정: 파라미터 없음 시 `411 ERR_NORECIPIENT PRIVMSG :대상 없음`.
    - 본문 없음: 대상만 있고 본문이 없을 때 `412 ERR_NOTEXTTOSEND :본문 없음`.
    - 닉네임 대상이 존재하지 않을 때: `401 ERR_NOSUCHNICK <target> :대상 없음`.
    - 채널 대상이 존재하지 않을 때: `403 ERR_NOSUCHCHANNEL <target> :채널 없음`.
    - 채널 대상이지만 발신자가 멤버가 아닐 때: `442 ERR_NOTONCHANNEL <target> :채널에 속해 있지 않음`.
  - 성공 시 동작:
    - 닉네임 대상: `:<sender> PRIVMSG <target> :<text>`를 대상 사용자에게 전송한다.
    - 채널 대상: 채널 멤버 전체(발신자 제외)에게 `:<sender> PRIVMSG <channel> :<text>`를 브로드캐스트한다.

- NOTICE
  - 문법: `NOTICE <target> :<text>` (target은 PRIVMSG와 동일 규칙)
  - 오류 처리: PRIVMSG와 동일한 조건/ numeric을 사용한다.
  - 성공 시 동작: PRIVMSG와 동일한 라인 형식(`NOTICE`)으로 전달하며, NOTICE도 채널 브로드캐스트 시 발신자를 제외한다.

- NAMES
  - 문법: `NAMES <channel>` (한 번에 하나의 채널만 지원)
  - 오류: 파라미터 없음 시 `461 ERR_NEEDMOREPARAMS NAMES :필수 파라미터 부족`, 채널 이름 규칙 위반 시 `476 ERR_BADCHANMASK <channel> :채널 이름 오류`.
  - 응답:
    - 채널이 존재하고 멤버가 있을 때: `:modern-irc 353 <nick> = <channel> :<nick1> <nick2> ...` (닉네임 공백 구분, 순서는 구현체 내부 순서).
    - 채널이 없으면 빈 목록으로 간주한다(353은 생략 가능).
    - 종료: `:modern-irc 366 <nick> <channel> :NAMES 종료`.

- LIST
  - 문법: `LIST` (v0.5.0에서는 파라미터 없는 전체 목록만 지원)
  - 응답:
    - 시작: `:modern-irc 321 <nick> Channel :Users Name`.
    - 각 채널: `:modern-irc 322 <nick> <channel> <usercount> :<topic>` (토픽이 없으면 `-`를 보낸다).
    - 종료: `:modern-irc 323 <nick> :LIST 종료`.

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

## 채널 규칙 및 JOIN/PART (v0.4.0, v0.7.0 보강)
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
  - 문법: `JOIN <channel> [<key>]`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS JOIN :필수 파라미터 부족`
  - 채널 이름 형식 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 이미 채널에 있는 경우: `443 ERR_USERONCHANNEL <nick> <channel> :이미 채널에 있음`
  - 모드 기반 거부:
    - 초대 전용(+i)인데 초대 목록에 없다면 `473 ERR_INVITEONLYCHAN <channel> :초대 전용`
    - 키(+k)가 설정돼 있고 키가 없거나 불일치하면 `475 ERR_BADCHANNELKEY <channel> :채널 키 불일치`
    - 인원 제한(+l)에 도달하면 `471 ERR_CHANNELISFULL <channel> :채널 인원 초과`
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

### v0.5.0에서 사용하는 numeric 추가 목록
- 401 ERR_NOSUCHNICK – PRIVMSG/NOTICE 대상 닉네임 없음
- 403 ERR_NOSUCHCHANNEL – PRIVMSG/NOTICE/NAMES 대상 채널 없음
- 411 ERR_NORECIPIENT – 대상 파라미터 없음
- 412 ERR_NOTEXTTOSEND – 본문 없음
- 321 RPL_LISTSTART – LIST 시작 안내
- 322 RPL_LIST – 채널 목록 항목
- 323 RPL_LISTEND – LIST 종료
- 353 RPL_NAMREPLY – NAMES 결과
- 366 RPL_ENDOFNAMES – NAMES 종료

---

## 채널 관리 및 권한 (v0.6.0)
- **오퍼레이터 부여 규칙**
  - 채널이 생성될 때 최초로 JOIN한 사용자가 해당 채널의 오퍼레이터가 된다.
  - 오퍼레이터가 떠나거나 KICK될 경우, 채널에 남은 첫 번째 멤버를 하나 선택해 즉시 오퍼레이터로 승격한다(채널이 비면 삭제된다).

- **TOPIC**
  - 조회 문법: `TOPIC <channel>`
    - 파라미터 없음: `461 ERR_NEEDMOREPARAMS TOPIC :필수 파라미터 부족`
    - 채널 이름 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
    - 채널 미존재: `403 ERR_NOSUCHCHANNEL <channel> :채널 없음`
    - 채널 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
    - 응답: 토픽이 없으면 `331 RPL_NOTOPIC <channel> :토픽 없음`, 있으면 `332 RPL_TOPIC <channel> :<topic>`
  - 설정 문법: `TOPIC <channel> :<topic>`
    - 파라미터 부족: `461 ERR_NEEDMOREPARAMS TOPIC :필수 파라미터 부족`
    - 채널 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
    - 권한 없음: 오퍼레이터가 아니면 `482 ERR_CHANOPRIVSNEEDED <channel> :채널 권한 없음`
    - 성공 시: `:<nick>!<user>@modern-irc TOPIC <channel> :<topic>`을 채널 전체에 브로드캐스트한다.

- **KICK**
  - 문법: `KICK <channel> <nick> [:<comment>]`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS KICK :필수 파라미터 부족`
  - 채널 이름 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 채널 미존재: `403 ERR_NOSUCHCHANNEL <channel> :채널 없음`
  - 호출자 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
  - 권한 없음: 호출자가 오퍼레이터가 아니면 `482 ERR_CHANOPRIVSNEEDED <channel> :채널 권한 없음`
  - 대상 미가입: `441 ERR_USERNOTINCHANNEL <nick> <channel> :대상이 채널에 없음`
  - 성공 시: `:<prefix> KICK <channel> <nick> :<comment>` 브로드캐스트 후 대상 멤버십을 제거한다. comment 없으면 `강퇴됨`을 사용한다.

- **INVITE**
  - 문법: `INVITE <nick> <channel>`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS INVITE :필수 파라미터 부족`
  - 채널 이름 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 채널 미존재: `403 ERR_NOSUCHCHANNEL <channel> :채널 없음`
  - 호출자 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
  - 권한 없음: 호출자가 오퍼레이터가 아니면 `482 ERR_CHANOPRIVSNEEDED <channel> :채널 권한 없음`
  - 대상이 이미 채널에 있을 때: `443 ERR_USERONCHANNEL <nick> <channel> :이미 채널에 있음`
  - 성공 시 흐름:
    - 호출자에게 `341 RPL_INVITING <nick> <channel>`을 보낸다.
    - 대상에게 `:<prefix> INVITE <nick> <channel>`을 전송한다.
    - 서버는 채널별 초대 목록에 대상 닉네임을 기록한다.(v0.7.0의 invite-only(+i) 대비)

### v0.6.0에서 사용하는 numeric 추가 목록
- 331 RPL_NOTOPIC – TOPIC 조회 시 토픽 없음
- 332 RPL_TOPIC – TOPIC 조회 시 토픽 반환
- 341 RPL_INVITING – INVITE 성공 알림
- 441 ERR_USERNOTINCHANNEL – KICK 시 대상 미가입
- 482 ERR_CHANOPRIVSNEEDED – 채널 오퍼레이터 권한 없음

---

## 채널 모드 및 MODE (v0.7.0)
- **지원 모드**: +i, +t, +k, +o, +l (채널 모드만 지원하며 사용자 모드는 미지원)
- **문법**: `MODE <channel> [<modestring> [<param1> [<param2> ...]]]`
  - `<modestring>`은 `+` 또는 `-`로 시작하며 지원 모드 문자만 포함해야 한다. 지원하지 않는 모드 문자는 `472 ERR_UNKNOWNMODE <char> :지원하지 않는 모드`로 거부한다.
  - `<param*>`는 모드 문자 순서대로 소진한다.
  - 파라미터가 필요한 모드에 값이 없으면 `461 ERR_NEEDMOREPARAMS MODE :필수 파라미터 부족`으로 거부한다.
- **호출 권한**
  - 채널 미가입자는 `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`으로 거부한다.
  - 모드 변경은 채널 오퍼레이터만 가능하며, 그렇지 않으면 `482 ERR_CHANOPRIVSNEEDED <channel> :채널 권한 없음`으로 거부한다.
  - `<modestring>` 없이 호출하면 현재 모드 상태를 `324 RPL_CHANNELMODEIS <channel> <modes> [<params>]` 형태로 반환한다.
- **모드별 동작/파라미터**
  - +i/-i (invite-only): 파라미터 없음. +i 시 초대 목록에 없는 사용자는 JOIN을 거부한다.
  - +t/-t (topic 보호): 파라미터 없음. +t이면 토픽 변경은 오퍼레이터만 가능하며, -t이면 채널 멤버 누구나 토픽을 설정할 수 있다. 초기 상태는 +t이다.
  - +k/-k (채널 키): +k는 키 문자열 1개 필수. -k는 파라미터 없이 키를 제거한다. 키가 설정된 채널은 JOIN 시 두 번째 파라미터로 정확한 키를 제시해야 한다.
  - +o/-o (오퍼레이터 부여/해제): 닉네임 1개 필수. 대상이 채널에 없으면 `441 ERR_USERNOTINCHANNEL <nick> <channel> :대상이 채널에 없음`으로 거부한다. -o로 오퍼레이터를 제거한 뒤 오퍼레이터가 비면 남은 멤버 중 첫 번째를 자동 승격시킨다.
  - +l/-l (인원 제한): +l은 양의 정수(10진수) 1개 필수. -l은 파라미터 없이 제한을 해제한다. 제한에 도달하면 추가 JOIN은 `471 ERR_CHANNELISFULL`로 거부한다.
- **브로드캐스트**
  - 모드가 정상 적용되면 `:<prefix> MODE <channel> <modestring> [params]`를 채널 멤버 전체(호출자 포함)에 브로드캐스트한다.

### v0.7.0에서 사용하는 numeric 추가 목록
- 324 RPL_CHANNELMODEIS – MODE 조회 응답
- 471 ERR_CHANNELISFULL – 채널 인원 초과
- 472 ERR_UNKNOWNMODE – 지원하지 않는 모드 문자
- 473 ERR_INVITEONLYCHAN – 초대 전용 채널 거부
- 475 ERR_BADCHANNELKEY – 채널 키 불일치

### v0.8.0에서 사용하는 numeric 추가 목록
- 382 RPL_REHASHING – REHASH 성공 알림
- 468 ERR_REHASHFAILED – 설정 파일 재적용 실패

---

## CRLF 보장
- 서버가 보내는 모든 응답 라인은 CRLF(`\r\n`)로 종료된다.
