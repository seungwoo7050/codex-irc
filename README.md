# modern-irc

C++17로 작성된 단일 스레드 IRC 서버 예제다. `poll()` 기반 이벤트 루프를 사용해 다중 클라이언트를 처리하며, 버전별로 기능을 확장한다. 외부 계약은 `design/protocol/contract.md`에 기록된다.

## v0.1.0 상태
- CRLF 프레이밍과 512바이트 길이 제한을 강제한다.
- 지원 명령: `PING <payload>` → `PONG <payload>`
- CLI: `./modern-irc <port> <password>`

## 빌드/테스트
자세한 절차는 `CLONE_GUIDE.md`와 `verify.sh`를 참고한다.

### 빠른 명령
```bash
./verify.sh
```
