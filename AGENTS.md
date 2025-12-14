# AGENTS.md
Repository seed rules for AI coding agents (C++17 IRC server, RFC 1459 기반).

> NOTE
> - This file is written in English for AI/tooling.
> - All generated docs and ALL source code comments MUST be written in Korean.

---

## 0) Seed vs Generated

### Seed (must exist at repo init)
- README.md (human overview)
- AGENTS.md
- STACK_DESIGN.md
- PRODUCT_SPEC.md
- CODING_GUIDE.md
- VERSIONING.md
- VERSION_PROMPTS.md
- CLONE_GUIDE.md (human, Korean; keep updated)

### Generated during development
- src/, include/, tests/
- design/** (Korean design docs)
- tools/** (text-only helper scripts)
- config/** (text-only)

---

## 1) Mandatory read order
Agents MUST read in this order before doing anything:
1. AGENTS.md
2. STACK_DESIGN.md
3. PRODUCT_SPEC.md
4. CODING_GUIDE.md
5. VERSIONING.md
6. VERSION_PROMPTS.md
7. README.md

---

## 2) Hard constraints

### 2.1 Language policy (MANDATORY)
- Code identifiers: English.
- ALL source code comments: Korean.
- ALL human-facing docs created/updated by agents: Korean.
  - CLONE_GUIDE.md
  - design/**
  - release notes / changelogs (if any)

This seed file may remain English.

### 2.2 Binary file policy (MANDATORY)
- Agents MUST NOT add or commit binary files.
- "Binary" means non-text files (archives, images, compiled outputs, logs, core dumps, etc.).
- Example (do NOT commit): *.o, *.a, *.so, *.dSYM, *.exe, *.ppm, *.png, logs/*
- If a file is generated locally, it MUST be gitignored.

### 2.3 Neutral naming policy (MANDATORY)
Use neutral technical naming only.
- Prefer: server, client, channel, command, parser, dispatcher, limiter, config, logger.
- Avoid narrative/theme-driven naming.

### 2.4 Stack immutability
Agents MUST follow STACK_DESIGN.md.
- No major stack changes (e.g., switching poll→epoll, adding heavy deps) without explicit human instruction AND seed doc update.

### 2.5 One target version per change set
Every change set MUST target exactly one version from VERSIONING.md.
- Do not mix v0.2.0 + v0.3.0 in one change set.

### 2.6 CI gate (MANDATORY from v0.1.0)
- From v0.1.0, the repository MUST include:
  - `./verify.sh` (single entrypoint for build + tests)
  - GitHub Actions workflow that runs `./verify.sh` on push/PR
- Any change that makes CI fail is invalid.
- Local reproduction MUST be: `./verify.sh`

---

## 3) External contract policy (MANDATORY)

### 3.1 Canonical contract file (fixed path)
The ONLY canonical external interface definition file is:
- `design/protocol/contract.md` (Korean)

This contract MUST be created in v0.1.0 and MUST be updated whenever any external interface changes:
- Supported IRC commands and their syntax
- Numeric replies/errors used by the server
- Connection/registration rules (PASS/NICK/USER sequencing)
- Channel mode semantics (when introduced)
- Server CLI arguments and config keys (when introduced)

### 3.2 Contract-first rule
If you introduce or change any external behavior:
1) Update `design/protocol/contract.md` FIRST
2) Then implement code
3) Then add tests that validate the contract

Implementation without contract update is invalid.

---

## 4) Testing policy (MANDATORY)

### 4.1 Do not rely on unit tests only
When external behavior exists/changes, provide BOTH:
- Unit tests (parser/state)
- Integration/E2E tests (real TCP connection; send IRC lines; assert replies)

### 4.2 Contract validation
Integration/E2E tests MUST validate:
- Reply line endings (CRLF)
- Essential numeric codes and their parameter shapes
- Essential channel/message routing behavior for introduced commands

---

## 5) Standard development loop (MANDATORY)
1) Select one target version from VERSIONING.md
2) Plan changes (list files to create/modify)
3) If external interface changes:
   - update design/protocol/contract.md first
4) Implement (Korean comments)
5) Add/update tests (unit + integration/E2E)
6) Run `./verify.sh` and fix until green (v0.1.0+)
7) Generate/update docs (Korean):
   - CLONE_GUIDE.md
   - design/**
8) Update VERSIONING.md status

---

## 6) Missing-info policy
- For internal details not specified: choose the minimal safe option without expanding scope.
- For external interfaces: you MUST document them in contract.md; do not improvise silently.
