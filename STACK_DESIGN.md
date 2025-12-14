# STACK_DESIGN.md
Authoritative stack and architecture baseline (C++17 IRC server, poll() 기반).

> NOTE
> - This file is written in English for AI/tooling.
> - All generated docs and code comments MUST be Korean.

---

## 1) Target system
Single C++17 binary that provides:
- RFC 1459/2810~2813 "subset" IRC server behavior
- TCP server with multi-client support via `poll()` (non-blocking)
- Text protocol parsing with CRLF framing

Non-goals (baseline):
- IRC server-to-server linking
- TLS/SASL/IRCv3 extensions

---

## 2) Approved baseline stack
- Language: C++17
- Networking: BSD sockets + `poll()`
- Build: Makefile (primary)
- Tests:
  - Unit: C++ (GoogleTest optional; or minimal custom harness)
  - Integration/E2E: Python 3 scripts (recommended) OR C++ socket tests
- No heavy external dependencies by default

---

## 3) Concurrency model (baseline rule)
- Single-process event loop.
- `poll()` drives accept/read/write.
- All protocol parsing + state mutation happens on the same thread.

Rationale:
- This project is about protocol/state correctness. `poll()` 기반 단일 스레드 구조가 디버깅과 결정성에 유리하다.

---

## 4) Protocol I/O baseline
### 4.1 Framing
- Messages are delimited by CRLF (`\r\n`).
- The server must tolerate partial TCP reads and buffer until CRLF.

### 4.2 Length limits
- Enforce IRC line length limits in the contract (default: 512 bytes including CRLF).
- Policy when exceeded must be documented (e.g., drop line + send error + disconnect).

### 4.3 Backpressure
- Each connection owns an outbound queue with a hard limit:
  - max queued messages OR max queued bytes (or both)
- On overflow, choose one policy (document in contract + tests):
  - (A) disconnect slow client
  - (B) drop low-priority messages
Baseline recommendation: (A) disconnect to keep invariants simple.

---

## 5) Core domain model (recommended)
### 5.1 Client
- fd
- registration state: pass_ok, nick_set, user_set, registered
- nick / username / realname
- input_buffer (std::string)
- outbound_queue (deque<string>)
- joined_channels (set<string>)
- user modes (if introduced)
- rate limiter (if introduced)

### 5.2 Channel
- name
- topic
- members (set<ClientId>)
- operators (set<ClientId>)
- modes:
  - +i (invite-only)
  - +t (topic settable by ops only)
  - +k (key)
  - +o (operator)
  - +l (user limit)

---

## 6) Module boundaries (target)
Recommended repo layout:

```
modern-irc/
  README.md
  AGENTS.md
  STACK_DESIGN.md
  PRODUCT_SPEC.md
  CODING_GUIDE.md
  VERSIONING.md
  VERSION_PROMPTS.md
  CLONE_GUIDE.md
  include/
  src/
    core/        (Server, Client, Channel)
    network/     (Socket, Poller)
    protocol/    (Message parse/format)
    commands/    (handlers)
    utils/       (Logger, Config, RateLimiter)
  tests/
    unit/
    e2e/
  design/
    protocol/contract.md
    server/
    ops/
  config/
  tools/
```

---

## 7) Observability baseline
- Logging:
  - Structured enough to trace: fd, nick (if known), command, error.
- No secrets in logs (passwords, oper pass).

---

## 8) Configuration baseline (optional by version)
- CLI: `./modern-irc <port> <password>`
- Optional config file (INI-like) later versions.
- If config exists, define reload rule (e.g., REHASH) in contract.
