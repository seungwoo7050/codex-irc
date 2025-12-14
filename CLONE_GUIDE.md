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
