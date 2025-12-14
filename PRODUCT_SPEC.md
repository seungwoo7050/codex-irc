# PRODUCT_SPEC.md
What to build (portfolio-oriented C++17 IRC server).

> NOTE
> - This file is written in English for AI/tooling.
> - All generated docs and code comments MUST be Korean.

---

## 1) Product goal (portfolio)
Demonstrate capability in:
- TCP server programming (BSD sockets)
- I/O multiplexing with `poll()`
- Text protocol parsing (CRLF framing, 512-byte limit)
- Stateful routing (users, channels, broadcasts)
- Defensive engineering (backpressure, rate limiting)
- Reproducible tests (unit + integration)

This is not a full IRCd; it is a correct, well-tested subset.

---

## 2) Functional scope (intentionally limited)

### Included (core)
- Connection lifecycle: accept/read/write/close
- Registration: PASS, NICK, USER
- Basic server commands: PING, PONG, QUIT
- Channels: JOIN, PART, NAMES, LIST
- Messaging: PRIVMSG, NOTICE

### Included (advanced, scheduled by VERSIONING)
- Channel operator & management: TOPIC, KICK, INVITE
- Modes (subset): +i +t +k +o +l (and minimal user modes if needed)
- Info commands: WHO, WHOIS (subset)
- Config + logging + rate limiting (as separate versions)

### Explicitly excluded
- Server-to-server linking
- Services (NickServ/ChanServ)
- IRCv3 extensions (tags, CAP, SASL)
- TLS (can be optional later, but not baseline)
- Persistence/DB (keep in-memory)

---

## 3) Non-functional requirements (must be demonstrable)
- Robustness:
  - partial TCP reads/writes handled correctly
  - malformed input does not crash the server
- Protocol hygiene:
  - CRLF framing
  - enforce max line length (define policy)
- Backpressure:
  - slow clients cannot cause unbounded memory growth
- Basic abuse prevention:
  - optional flood/rate limiting per connection
- Memory safety:
  - valgrind clean (or equivalent)

---

## 4) Definition of Done (per version)
A version is DONE only if:
- VERSIONING.md status is updated
- contract.md matches implementation
- tests are green (unit + integration for external behavior)
- CLONE_GUIDE.md contains exact reproduction steps
