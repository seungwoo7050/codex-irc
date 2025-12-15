# design/protocol/contract.md

## 개요 (v1.0.0)
- 본 문서는 modern-irc 서버의 외부 프로토콜 계약을 정의하며 v1.0.0에서 동결된다.
- v1.0.0은 신규 기능 추가 없이 호환성·문서·테스트 정합성을 확정하는 안정화 릴리스다.
- 지원/미지원 범위
  - **지원 명령**: PASS, NICK, USER, PING, PONG, QUIT, JOIN, PART, PRIVMSG, NOTICE, NAMES, LIST, TOPIC, KICK, INVITE, MODE(+i/+t/+k/+o/+l), REHASH
  - **명시적 미지원**: WHO/WHOIS/WHOWAS 등 확장 조회, 사용자 모드, 서버 링크, TLS/SASL/IRCv3 태그, 서비스 계정(NickServ/ChanServ), 서버 간 명령 확장

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
    - `name` (기본: `modern-irc`): numeric prefix와 사용자 prefix의 호스트 부분에 사용된다.
  - `[logging]`
    - `level` (기본: `info`, 허용: `debug|info|warn|error`, 대소문자 무시)
    - `file` (기본: 빈 문자열 → 표준 오류로 출력, `-`도 표준 오류 의미)
  - `[limits]`
    - `messages_per_5s` (기본: `0` → 비활성화): 5초 윈도우 동안 허용되는 PRIVMSG/NOTICE 전송 횟수 상한.
    - `outbound_lines` (기본: `16`): 송신 큐 상한(라인 수). 0 또는 누락 시 기본값 사용.
- 설정 파일이 없으면 모든 키가 기본값으로 채워진다.
- 파일이 존재하지만 구문/값이 잘못되면 로드에 실패하며, 실패 시 이전 구성이 유지된다.

### 로깅 및 리로드
- 로그 레벨: debug < info < warn < error 순서로 필터링한다.
- 출력 대상: `logging.file`이 비어 있거나 `-`이면 표준 오류로 기록하며, 경로가 주어지면 append 모드로 파일을 연다.
- REHASH 또는 SIGHUP으로 설정을 다시 읽으면 새 로그 설정과 서버명이 즉시 반영된다.

---

## 입력/출력 프레이밍
- 메시지 구분자는 CRLF(`\r\n`)이며, 서버가 전송하는 모든 응답도 CRLF로 끝난다.
- 각 클라이언트는 개별 입력 버퍼를 가지며 부분 수신을 허용한다.
- 라인 최대 길이: **512바이트(종료 CRLF 포함)**
  - CRLF를 찾았을 때 해당 라인이 512바이트를 초과하면 즉시 연결을 종료한다(에러 라인 전송 없음).
  - CRLF가 오기 전에 버퍼가 512바이트를 넘으면 버퍼를 비우고 연결을 종료한다.
- 메시지 파싱 규칙:
  - prefix: 라인이 `:`로 시작하면 prefix는 다음 공백 전까지이며, 이후 공백은 모두 스킵한다.
  - command: prefix 이후 첫 토큰. 서버 내부에서는 대문자로 정규화한다.
  - params: command 뒤 공백으로 구분된 토큰들. `:`로 시작하는 토큰은 해당 위치부터 라인 끝까지를 단일 trailing 파라미터로 취급한다. 연속 공백은 무시하며 빈 파라미터는 생성하지 않는다.

---

## 출력 큐(백프레셔)
- 각 클라이언트는 송신 대기열(deque<string>)을 가진다.
- 상한: 기본 16개 라인(`limits.outbound_lines`), 5초 윈도우로 큐잉 내역을 추적한다.
- 새 라인을 추가하려 할 때 상한을 넘으면 큐 주인 클라이언트를 로그에 남기고 즉시 종료하며, 초과한 라인은 전송하지 않는다.

## 레이트리밋
- 적용 대상: 등록된 클라이언트가 발신하는 PRIVMSG/NOTICE.
- 파라미터: `[limits] messages_per_5s` 값을 최대 횟수로 사용하며, 윈도우는 고정 5초이다. 값이 0이면 레이트리밋을 비활성화한다.
- 위반 시 정책:
  - 위반 명령은 드롭된다(브로드캐스트/개별 전달 없음).
  - 호출자에게 `439 ERR_RATEEXCEEDED <nick> :발송 속도 초과` numeric을 전송한다.
  - 연결은 유지되며, 이후 윈도우에서 다시 허용될 때까지 동일 정책을 반복한다.

---

## 등록 흐름
- 필수 입력: PASS(정확한 비밀번호), NICK(유효한 닉네임), USER(사용자명, realname).
- 등록 완료 조건: PASS 성공 + NICK 설정 + USER 설정을 모두 만족하면 즉시 registered 상태로 전환하고 환영 numeric을 전송한다.
- 허용 순서: PASS/NICK/USER는 순서 무관하지만 PASS는 등록 완료 전에만 허용된다.
- 닉네임 제약: 영문/숫자/`[]\\`_`-` 만 허용하며, 첫 글자는 영문 또는 숫자여야 한다.
- 중복 닉네임: 이미 등록된 동일 닉네임이 있으면 거부한다.

### 등록 전 허용/거부 커맨드
- 허용: PASS, NICK, USER, PING, PONG, QUIT
- 거부: 그 외 모든 커맨드(JOIN, PRIVMSG 등)는 `451 ERR_NOTREGISTERED :등록 필요`로 거부한다.

### 등록 성공/실패 numeric
- 성공: `:<server> 001 <nick> :등록 완료` (`server`는 설정 서버명)
- PASS 오류:
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS PASS :필수 파라미터 부족` 전송 후 연결 종료
  - 비밀번호 불일치: `464 ERR_PASSWDMISMATCH :비밀번호 불일치` 전송 후 연결 종료
  - 이미 등록: `462 ERR_ALREADYREGISTRED :이미 등록됨`
- NICK 오류:
  - 파라미터 없음: `431 ERR_NONICKNAMEGIVEN :닉네임 없음`
  - 형식 오류: `432 ERR_ERRONEUSNICKNAME <nick> :닉네임 형식 오류`
  - 중복: `433 ERR_NICKNAMEINUSE <nick> :닉네임 사용 중`
  - 이미 등록 후 재시도: `462 ERR_ALREADYREGISTRED :이미 등록됨`
- USER 오류:
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS USER :필수 파라미터 부족`
  - 이미 등록: `462 ERR_ALREADYREGISTRED :이미 등록됨`

---

## 공통 규칙
- 채널 이름: `#`로 시작, 길이 2~50, 영문/숫자/`_`/`-`만 허용. 위반 시 `476 ERR_BADCHANMASK`.
- 연결 종료/QUIT 시 처리: 사용자가 속했던 각 채널에 `:<nick>!<user>@<server> PART <channel> :연결 종료`를 브로드캐스트한 뒤 멤버십을 제거한다.
- 등록 완료 후 지원하지 않는 명령을 호출하면 `421 ERR_UNKNOWNCOMMAND <cmd> :알 수 없는 명령`을 반환한다.

---

## 명령별 계약

### PING
- 요청: `PING <payload>`
- 응답: `PONG <payload>`\r\n
- `<payload>` 없음: `409 ERR_NOORIGIN :출처 없음`

### PONG
- 요청: `PONG <payload>`
- 응답: 별도 응답 없음. `<payload>` 없음 시 `409 ERR_NOORIGIN :출처 없음`

### QUIT
- 요청: `QUIT [:<message>]`
- 동작: 입력 여부와 무관하게 연결 종료. 추가 브로드캐스트 없음(채널 정리는 공통 규칙 참조).

### PASS
- 요청: `PASS <password>`
- 동작: 등록 완료 전에만 허용. 비밀번호가 맞아야 등록 조건을 충족한다. 오류/파라미터 부족 시 numeric 후 연결 종료.

### NICK
- 요청: `NICK <nickname>`
- 동작: 닉네임 설정. 유효성/중복 검사 후 성공 시 등록 상태 갱신.

### USER
- 요청: `USER <username> 0 * :<realname>`
- 동작: 사용자명/realname 설정. 파라미터 부족 시 거부.

### JOIN
- 요청: `JOIN <channel> [<key>]`
- 오류:
  - 등록 전: `451 ERR_NOTREGISTERED`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS JOIN :필수 파라미터 부족`
  - 채널 이름 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 이미 가입: `443 ERR_USERONCHANNEL <channel> :이미 채널에 있음`
  - invite-only(+i) 미초대: `473 ERR_INVITEONLYCHAN <channel> :초대 전용`
  - +k 키 불일치: `475 ERR_BADCHANNELKEY <channel> :채널 키 불일치`
  - +l 인원 초과: `471 ERR_CHANNELISFULL <channel> :채널 인원 초과`
- 성공 시:
  - 채널이 비어 있으면 호출자를 오퍼레이터로 등록한다.
  - 초대 목록에 있었다면 초대 정보를 지우고 입장시킨다.
  - `:<nick>!<user>@<server> JOIN <channel>`을 채널 구성원 전체(자신 포함)에 브로드캐스트한다.

### PART
- 요청: `PART <channel> [:<message>]`
- 오류:
  - 등록 전: `451 ERR_NOTREGISTERED`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS PART :필수 파라미터 부족`
  - 채널 이름 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
  - 채널 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
- 성공 시: `:<nick>!<user>@<server> PART <channel> :<message>`를 채널 전체에 브로드캐스트하고 멤버십을 제거한다. `<message>`가 없으면 `사용자 요청`을 사용한다. 채널이 비면 삭제한다.

### PRIVMSG / NOTICE
- 요청: `PRIVMSG <target> :<text>` 또는 `NOTICE <target> :<text>`
- 오류:
  - 등록 전: `451 ERR_NOTREGISTERED`
  - 대상 없음: `411 ERR_NORECIPIENT <command> :대상 없음`
  - 본문 없음: `412 ERR_NOTEXTTOSEND :본문 없음`
  - 레이트리밋 초과: `439 ERR_RATEEXCEEDED <nick> :발송 속도 초과`
- 대상이 채널인 경우:
  - 채널 이름 오류 또는 존재하지 않음: `403 ERR_NOSUCHCHANNEL <channel> :채널 없음`
  - 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
  - 성공 시 `:<prefix> PRIVMSG/NOTICE <channel> :<text>`를 채널 구성원에게 브로드캐스트(발신자 제외).
- 대상이 닉네임인 경우:
  - 대상 미존재: `401 ERR_NOSUCHNICK <nick> :대상 없음`
  - 성공 시 해당 사용자에게만 전달.

### NAMES
- 요청: `NAMES <channel>`
- 오류:
  - 등록 전: `451 ERR_NOTREGISTERED`
  - 파라미터 부족: `461 ERR_NEEDMOREPARAMS NAMES :필수 파라미터 부족`
  - 채널 이름 오류: `476 ERR_BADCHANMASK <channel> :채널 이름 오류`
- 응답: 채널이 존재하고 멤버가 있으면 `353 RPL_NAMREPLY <nick> = <channel> :<members>` 전송 후, 항상 `366 RPL_ENDOFNAMES <channel> :NAMES 종료`로 종료한다.

### LIST
- 요청: `LIST`
- 오류: 등록 전 `451 ERR_NOTREGISTERED`
- 응답: `321 RPL_LISTSTART <nick> Channel :Users Name` → 각 채널에 대해 `322 RPL_LIST <nick> <channel> <count> :<topic|- >` → `323 RPL_LISTEND <nick> :LIST 종료`.

### TOPIC
- 조회: `TOPIC <channel>`
  - 오류: 파라미터 부족(461), 채널 이름 오류(476), 채널 없음(403), 미가입(442)
  - 응답: 토픽 없으면 `331 RPL_NOTOPIC <channel> :토픽 없음`, 있으면 `332 RPL_TOPIC <channel> :<topic>`
- 설정: `TOPIC <channel> :<topic>`
  - 오류: 파라미터 부족(461), 미가입(442), 권한 없음(482; +t 상태에서는 오퍼레이터만 허용)
  - 성공 시 `:<prefix> TOPIC <channel> :<topic>`을 채널 전체에 브로드캐스트하며, 토픽은 LIST 결과에도 반영된다. 기본 모드는 +t로 시작한다.

### KICK
- 요청: `KICK <channel> <nick> [:<comment>]`
- 오류: 파라미터 부족(461), 채널 이름 오류(476), 채널 없음(403), 호출자 미가입(442), 권한 없음(482), 대상 미가입(441)
- 성공 시: `:<prefix> KICK <channel> <nick> :<comment>` 브로드캐스트 후 대상 멤버십 제거. comment가 없으면 `강퇴됨` 사용.

### INVITE
- 요청: `INVITE <nick> <channel>`
- 오류: 파라미터 부족(461), 채널 이름 오류(476), 채널 없음(403), 호출자 미가입(442), 권한 없음(482), 대상이 이미 채널에 있음(443)
- 성공 시: 호출자에게 `341 RPL_INVITING <nick> <channel>` 전송, 대상에게 `:<prefix> INVITE <nick> <channel>` 전송, 초대 목록에 대상 닉을 기록한다(+i 대비).

### MODE (채널)
- 문법: `MODE <channel> [<modestring> [<params>...]]`
- 지원 모드: +i, +t, +k, +o, +l (사용자 모드 미지원)
- 조회: `<modestring>` 없이 호출하면 `324 RPL_CHANNELMODEIS <channel> <modes> [params]`로 현재 모드/키/인원 제한을 반환한다.
- 오류:
  - 등록 전: `451 ERR_NOTREGISTERED`
  - 파라미터 부족: 모드 변경 시 필요한 파라미터가 없으면 `461 ERR_NEEDMOREPARAMS MODE :필수 파라미터 부족`
  - 채널 미가입: `442 ERR_NOTONCHANNEL <channel> :채널에 속해 있지 않음`
  - 권한 없음: 모드 변경은 오퍼레이터만 가능하며, 아니면 `482 ERR_CHANOPRIVSNEEDED <channel> :채널 권한 없음`
  - 알 수 없는 모드 문자가 포함되면 `472 ERR_UNKNOWNMODE <char> :지원하지 않는 모드`
- 모드별 동작:
  - +i/-i: invite-only 토글. +i면 초대 목록에 없는 사용자의 JOIN을 `473`으로 거부.
  - +t/-t: 토픽 보호 토글. +t에서만 오퍼레이터가 토픽 변경 가능(기본 +t).
  - +k/-k: 채널 키 설정/해제. +k는 키 1개 필요, -k는 키 제거(파라미터 없음). 키가 설정되면 JOIN 시 두 번째 파라미터로 정확한 키를 요구하며 불일치 시 `475`.
  - +o/-o: 오퍼레이터 부여/해제. 닉이 채널에 없으면 `441 ERR_USERNOTINCHANNEL`. -o 이후 오퍼레이터가 없으면 남은 첫 멤버를 자동 승격.
  - +l/-l: 인원 제한 설정/해제. +l은 양의 정수 필요, -l은 파라미터 없이 제한 해제. 제한 도달 시 JOIN을 `471`로 거부.
- 모드 적용 시 `:<prefix> MODE <channel> <modestring> [params]`를 채널 전체에 브로드캐스트한다.

### REHASH / SIGHUP
- 요청: `REHASH`
- 조건: 등록 완료 사용자만 호출 가능.
- 성공: 설정을 다시 읽고 `382 RPL_REHASHING <path> :설정 리로드 완료`
- 실패: 설정 파싱 오류 시 `468 ERR_REHASHFAILED <path> :<사유>` (기존 설정 유지)
- SIGHUP 수신 시 REHASH와 동일한 동작을 수행하며, 성공/실패 로그만 남긴다.

---

## CRLF 보장
- 서버가 보내는 모든 응답 라인은 CRLF(`\r\n`)로 종료된다.
