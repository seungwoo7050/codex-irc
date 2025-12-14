# CODING_GUIDE.md
Coding rules for this repository (C++17 IRC server).

> NOTE
> - This file is written in English for AI/tooling.
> - All generated docs and code comments MUST be Korean.

---

## 1) Global rules

### 1.1 Language
- Identifiers: English.
- Comments: Korean only.

### 1.2 No binaries in repo
- Do not commit non-text files.
- Rendered outputs/logs must remain local.

### 1.3 Required file header comment (Korean)
For significant `.hpp/.cpp` files add:
- 설명(역할)
- 버전: vX.Y.Z
- 관련 문서 경로(있으면)
- 테스트 경로(있으면)

Example:
/*
 * 설명: IRC 메시지 파싱과 정규화를 담당한다.
 * 버전: v0.3.0
 * 관련 문서: design/protocol/contract.md
 * 테스트: tests/unit/message_parser_test.cpp
 */

---

## 2) C++17 rules
- Standard: C++17 only.
- Prefer RAII; avoid owning raw pointers.
- Avoid throwing exceptions across the poll loop; handle errors explicitly.
- No blocking calls that can stall the event loop (DNS, file IO in hot path, etc.).

---

## 3) Networking & event loop rules

### 3.1 Non-blocking sockets
- Set listening socket and client sockets non-blocking.
- Treat EAGAIN/EWOULDBLOCK as "try later".

### 3.2 Buffering policy
- Each client has:
  - input buffer (accumulate until CRLF)
  - outbound queue (deque<string>)
- Never assume one `recv()` == one IRC line.

### 3.3 CRLF framing & line length
- Split by `\r\n`.
- Enforce max line length as defined in `design/protocol/contract.md`.
- If exceeded: follow documented policy (e.g., ERR_INPUTTOOLONG + disconnect).

### 3.4 Outbound backpressure
- Outbound queue must have a hard limit (bytes or messages).
- Overflow behavior must be deterministic and tested.

---

## 4) Protocol & command handling

### 4.1 Message parsing
- Support:
  - optional prefix
  - command token
  - params (including trailing `:<text>`)
- Command token matching should be case-insensitive.

### 4.2 Registration state machine
- Clearly define when a client becomes "registered".
- Reject/ignore commands not allowed before registration, per contract.

### 4.3 Dispatch style
- Use a table-based dispatcher:
  - command string -> handler function
- Handler signature should not directly write to socket; return replies or push to outbound queue.

### 4.4 Numeric replies
- Centralize numeric formatting.
- Do not inline numbers all over the code; define constants/enums.

---

## 5) Testing rules

### 5.1 Unit tests
- Parser: raw line -> parsed fields
- Channel state: JOIN/PART invariants
- Formatting: numeric replies shape

### 5.2 Integration/E2E tests
- Spawn server on an ephemeral port.
- Open TCP socket(s), send IRC lines, assert replies (including CRLF).
- For routing commands (PRIVMSG, JOIN): use 2+ clients and assert broadcast.

---

## 6) Documentation rules
- External behavior changes => update `design/protocol/contract.md` first.
- After tests are green, update:
  - CLONE_GUIDE.md
  - design/server/vX.Y.Z-*.md (short is OK)
