# CLONE_GUIDE.md

## 개요
`modern-irc`를 로컬에서 빌드·기동·E2E 스모크까지 재현하기 위한 가이드다. 아래 순서를 그대로 따르면 v1.0.0 동작을 확인할 수 있다.

---

## 요구 사항
- C++17 컴파일러 (Linux: g++, macOS: clang++)
- make
- Python 3 (E2E 스모크 스크립트 실행용)

---

## 1) 전체 검증
```bash
./verify.sh
```
- 단위 테스트 + E2E 스모크를 모두 실행한다.
- 오류가 나면 로그를 확인 후 아래 수동 단계로 재현한다.

## 2) 수동 빌드/실행
```bash
make
./modern-irc <port> <password> [config_path]

# 예시
./modern-irc 6667 testpass
```
- 프로세스는 포그라운드에서 실행된다. 새 터미널을 열어 클라이언트를 붙인다.

### 2-1) 설정 파일 예시
`config/server.ini` 또는 임의 경로를 만들어 `[config_path]`로 넘길 수 있다.

```ini
[server]
name=custom-irc
[logging]
level=warn
file=-
[limits]
messages_per_5s=3
outbound_lines=16
```
- `name`: numeric prefix와 사용자 prefix 호스트에 사용된다.
- `level`: debug/info/warn/error 중 하나.
- `file`: 로그 출력 경로(비우거나 `-`면 표준 오류).
- `messages_per_5s`: 5초당 허용되는 PRIVMSG/NOTICE 횟수. 초과 시 `439`로 드롭된다.
- `outbound_lines`: 송신 큐 상한. 초과 시 연결이 종료된다.
- 설정을 수정했다면 실행 중인 서버에 `REHASH`를 보내 즉시 반영할 수 있다.

---

## 3) 기본 스모크 체크
터미널을 두 개 열고 모두 `nc localhost <port>`로 접속한다고 가정한다(입력은 반드시 `\r\n`으로 끝나야 한다).

### 3-1) 등록(PASS/NICK/USER)
터미널 A에서 아래를 순서 무관하게 입력한다.
```
PASS testpass
NICK hero
USER hero 0 * :Real Hero
```
- `:custom-irc 001 hero :등록 완료`가 오면 등록 완료다. 비밀번호가 틀리면 `464` 후 연결이 닫힌다.

### 3-2) 채널 JOIN/PART 브로드캐스트
터미널 A에서 `JOIN #room`을 보내면 `:hero!hero@custom-irc JOIN #room` 형태의 알림이 돌아온다.
터미널 B에서 등록 후 `JOIN #room`을 보내면 두 터미널 모두 JOIN 브로드캐스트를 받는다.
터미널 A에서 `PART #room :bye`를 보내면 B에서 PART 브로드캐스트를 받고, 이후 A는 더 이상 채널 알림을 받지 않는다.

### 3-3) 채널 PRIVMSG 브로드캐스트
두 터미널 모두 `JOIN #room` 상태에서 A가 `PRIVMSG #room :hello all`을 보내면 B가 해당 라인을 받고, A에게는 돌아오지 않는다.
닉네임 대상으로 `PRIVMSG hero :hi`를 보내면 대상 사용자만 수신한다.

### 3-4) MODE(+k 또는 +i) 정책
A가 오퍼레이터인 상태에서 아래 중 하나를 검증한다.
- 키 모드: `MODE #room +k secret` → 확인 라인 수신 후, B가 `JOIN #room wrong` 시 `475`를 받고 `JOIN #room secret` 시 성공한다.
- 초대 전용: `MODE #room +i` 적용 후 초대하지 않은 사용자가 JOIN하면 `473`으로 거부된다.

### 3-5) REHASH
서버 실행 시 사용한 설정 파일을 수정한 뒤, 등록된 터미널에서 `REHASH`를 보내면 `382`가 돌아오며 이후 numeric prefix가 새 서버명으로 반영된다.

---

## 4) 테스트 직접 실행
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
- 레이트리밋/송신 큐 제한에 걸렸다면 설정 파일의 `messages_per_5s`와 `outbound_lines`를 늘린 뒤 REHASH를 수행한다.
