# CLONE_GUIDE.md

## 개요
`modern-irc`를 로컬에서 빌드·기동·간단 검증(E2E 스모크)까지 한 번에 재현하기 위한 가이드다.

> 이 문서는 사람용(한국어)이며, 버전이 올라갈수록 실제 동작에 맞게 계속 갱신한다.

---

## 요구 사항
- C++17 컴파일러 (Linux: g++, macOS: clang++)
- make
- (권장) netcat(nc) 또는 irssi/weechat/hexchat 같은 IRC 클라이언트
- (선택) Python 3 (E2E 스모크 스크립트가 있을 경우)

---

## 빠른 시작

### 1) 빌드
```bash
make
```

### 2) 서버 실행
```bash
./modern-irc <port> <password>

# 예시
./modern-irc 6667 testpass
```

### 3) nc로 접속(수동 스모크)
```bash
nc localhost 6667
PASS testpass
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :Hello, world!
```

---

## irssi로 접속(선택)
```bash
irssi -c localhost -p 6667
/PASS testpass
/NICK alice
/JOIN #general
```

---

## 테스트 실행(프로젝트가 제공하는 경우)
프로젝트 버전에 따라 테스트 커맨드는 달라질 수 있다.

- (예시) 단위 테스트:
  ```bash
  make test
  ```
- (예시) E2E 스모크(Python):
  ```bash
  python3 tools/e2e/smoke.py --host 127.0.0.1 --port 6667 --pass testpass
  ```

> 어떤 테스트가 "필수"인지 는 `VERSIONING.md`/`VERSION_PROMPTS.md`에 정의된다.

---

## 설정 파일(버전별로 도입)
설정 파일 기능이 들어간 버전에서는 보통 다음 형태를 따른다.

- 위치(예시): `config/modern-irc.conf`
- 형식: INI 스타일(key=value)

예시:
```ini
server_name = irc.local
port = 6667
max_clients = 1000

server_password = testpass

log_enabled = true
log_level = INFO
log_file = logs/modern-irc.log
```

> `logs/`와 같은 런타임 산출물은 **커밋 금지**(gitignore).

---

## 문제 해결
- 포트가 이미 사용중이면:
  - 다른 포트를 사용하거나, 기존 프로세스를 종료한다.
- 접속은 되는데 명령이 먹지 않으면:
  - CRLF(\r\n) 프레이밍이 지켜지는지 확인한다(nc는 보통 OK).
  - 등록(PASS/NICK/USER) 순서가 `design/protocol/contract.md`와 일치하는지 확인한다.
