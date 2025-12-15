# CLONE_GUIDE.md

## 개요
`modern-irc`를 로컬에서 빌드·기동·E2E 스모크까지 재현하기 위한 가이드다.

> 이 문서는 사람용(한국어)이며, 버전이 올라갈수록 실제 동작에 맞게 계속 갱신한다.

---

## 요구 사항
- C++17 컴파일러 (Linux: g++, macOS: clang++)
- make
- Python 3 (E2E 스모크 스크립트 실행용)

---

## 빠른 시작

### 1) 전체 검증
```bash
./verify.sh
```

### 2) 수동 빌드/실행
```bash
make
./modern-irc <port> <password>

# 예시
./modern-irc 6667 testpass
```

### 3) nc로 간단 스모크
```bash
nc localhost 6667
PING hello
```
- 서버 응답: `PONG hello` (CRLF 포함)

### 4) 등록/접속 예시
```bash
nc localhost 6667
PASS testpass
NICK hero
USER user 0 * :Real User
```
- 순서와 무관하게 PASS/NICK/USER를 모두 보내면 `:modern-irc 001 hero :등록 완료` 응답이 돌아오면 성공이다.

### 5) 채널 입장/퇴장 스모크
- 터미널 A에서 등록 후 `JOIN #room`을 보내면 `JOIN #room` 알림이 돌아온다.
- 터미널 B에서 같은 채널에 JOIN 하면 터미널 A/B 모두 `JOIN #room` 알림을 받는다.
- 터미널 A에서 `PART #room :bye`를 보내면 터미널 B에서 `PART #room` 브로드캐스트를 받은 뒤, 이후 새 사용자가 JOIN해도 터미널 A는 더 이상 채널 알림을 받지 않는다.

### 6) 메시징/NAMES/LIST 스모크
- 두 명이 등록 후 각각 터미널 A/B에서 `PRIVMSG <상대닉> :hello`를 보내면 상대만 해당 라인을 수신한다.
- 채널에 함께 JOIN한 뒤 `PRIVMSG #room :hi all`을 보내면 채널 구성원(발신자 제외)에게 브로드캐스트된다.
- 채널에서 `NAMES #room`을 호출하면 `353` 라인에 구성원 닉네임이 공백으로 나열되고 `366`으로 종료된다.
- `LIST`를 호출하면 `321` 시작 → `322 <채널> <인원수> :-` → `323` 종료 순으로 채널 목록이 반환된다.

### 7) 채널 관리 스모크(TOPIC/KICK/INVITE)
- 채널을 새로 만든 사용자(첫 JOIN)가 오퍼레이터다. 다른 사용자는 `482`가 나오면 오퍼레이터에게 권한을 요청해야 한다.
- 토픽 설정: 오퍼레이터가 `TOPIC #room :환영합니다`를 보내면 채널 구성원 모두가 `TOPIC #room :환영합니다` 브로드캐스트를 받는다. 멤버가 `TOPIC #room`을 조회하면 `332` 응답이 돌아온다.
- 강퇴: 오퍼레이터가 `KICK #room <닉> :사유`를 보내면 채널 구성원 모두가 KICK 라인을 받고 대상은 멤버십에서 제거된다. 권한 없는 사용자는 `482`를 받는다.
- 초대: 오퍼레이터가 `INVITE <닉> #room`을 보내면 `341` 확인을 받고, 대상 사용자는 `INVITE <닉> #room` 알림을 받은 뒤 `JOIN #room`으로 입장할 수 있다.

### 8) MODE 스모크(+i/+t/+k/+o/+l)
- `MODE #room`으로 현재 모드(기본 +t 포함)를 324 numeric으로 확인할 수 있다.
- 초대 전용: `MODE #room +i` 적용 후 다른 사용자가 초대 없이 JOIN하면 `473`으로 거부된다.
- 채널 키: `MODE #room +k <key>` 적용 후 JOIN `<channel> <key>`가 아니면 `475`로 거부된다.
- 인원 제한: `MODE #room +l 1`처럼 제한을 걸면 제한 인원 이후 JOIN 시 `471`이 반환된다.
- 오퍼레이터 부여/해제: `MODE #room +o <nick>` 또는 `-o <nick>`으로 조정하며, 오퍼레이터가 모두 사라지면 남은 첫 멤버가 자동 승격된다.

---

## 테스트 실행
- 단위 테스트:
  ```bash
  make test
  ```
- E2E 스모크:
  ```bash
  make e2e
  ```

---

## 문제 해결
- 포트가 이미 사용 중이면 다른 포트를 사용하거나 기존 프로세스를 종료한다.
- 응답이 오지 않으면 입력 라인이 CRLF(`\r\n`)로 끝나는지 확인한다.
