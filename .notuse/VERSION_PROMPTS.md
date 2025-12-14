# VERSION_PROMPTS.md

modern-irc (C++17 IRC 서버)용 "버전별 복붙 프롬프트" 모음.

사용 방법:
- 아래에서 대상 버전 섹션(예: v0.3.0)을 통째로 복사해서 코딩 에이전트에게 전달한다.
- 한 번에 반드시 **1개 버전만** 진행한다(버전 혼합 금지).
- 외부 동작(지원 명령/응답/정책)을 바꾸면 구현 전에 `design/protocol/contract.md`부터 고정한다.

---

## 병렬 개발(Modern IRC + Ray Tracer)용 마스터 프롬프트 (선택)

두 프로젝트를 "같은 0.1.0 스텝"으로 병렬 개발하려면 아래 템플릿을 사용한다.
- 멀티 워크스페이스(또는 2개 에이전트) 환경에서:
  - Workspace A: modern-irc 저장소
  - Workspace B: ray-tracer 저장소

```text
[Workspace A: modern-irc]
대상 버전: v0.X.0
→ modern-irc 저장소의 VERSION_PROMPTS.md에서 v0.X.0 섹션 전체를 실행하라.

[Workspace B: ray-tracer]
대상 버전: v0.X.0
→ ray-tracer 저장소의 VERSION_PROMPTS.md에서 v0.X.0 섹션 전체를 실행하라.

공통 금지:
- 버전 혼합 금지(각 저장소는 정확히 1개 버전만)
- 바이너리 커밋 금지
- 식별자 영어, 주석/사람용 문서 한국어
```

---

## 공통 프롬프트(항상 포함)

```text
레포 루트의 문서를 다음 순서로 모두 읽어라:
AGENTS.md → STACK_DESIGN.md → PRODUCT_SPEC.md → CODING_GUIDE.md → VERSIONING.md → VERSION_PROMPTS.md → README.md

반드시 VERSIONING.md에서 "버전 1개"만 선택해서 작업해라. (버전 혼합 금지)

바이너리 파일은 절대 커밋하지 마라.
코드 식별자는 영어, 모든 주석/사람용 문서는 한국어로 작성해라.

외부 동작(지원 IRC 명령/응답/정책)을 추가/변경하면 구현 전에
`design/protocol/contract.md`(한국어)부터 먼저 만들어/갱신해서 고정해라.
그 문서와 불일치하는 구현/테스트는 금지다.

작업 순서:
1) 계획(생성/수정 파일 목록)
2) (필요 시) contract.md 선 고정
3) 구현
4) 테스트(단위 + 통합/E2E)
5) 문서 생성/갱신(CLONE_GUIDE.md, design/*)
6) VERSIONING.md 상태 갱신
```

---

## v0.1.0 프롬프트 — 스켈레톤 + 계약 고정 + TCP/E2E 스모크

```text
대상 버전: v0.1.0

목표:
- include/src/tests/design/tools 뼈대를 만든다.
- poll() 기반 TCP 서버가 다중 클라이언트를 수락/해제할 수 있다.
- 구현 전에 `design/protocol/contract.md`(한국어)를 생성하고 최소 규약을 고정한다:
  - CRLF 프레이밍 정책(부분 수신/버퍼링)
  - 최대 라인 길이(512 포함 여부)와 초과 시 정책
  - v0.1.0에서 지원하는 "최소 동작"(예: 임시 ECHO 명령 또는 PING 응답) 이름/형태
  - 서버 실행 CLI: ./modern-irc <port> <password> (또는 동등)

구현:
- STACK_DESIGN.md의 스택만 사용(poll + BSD sockets).
- per-connection input buffer/outbound queue를 반드시 둔다.
- outbound queue에 상한을 두고 초과 정책을 문서+테스트로 고정한다.

테스트(필수):
- 단위 테스트: 1개 이상
  - 예: CRLF 프레임 분리 함수가 조각난 입력에서도 정상 동작
- E2E 스모크: 1개 이상
  - 서버를 띄우고 TCP로 연결
  - 최소 1개 라인을 보내고 서버 응답 1회 수신
  - 응답의 CRLF 포함 여부 확인

산출 문서(테스트 통과 후):
- CLONE_GUIDE.md(한국어): 빌드/실행/스모크 테스트 방법
- design/server/v0.1.0-overview.md(한국어): 모듈/이벤트루프 요약(짧아도 됨)
- design/protocol/contract.md(한국어): v0.1.0 범위 정본

완료 처리:
- VERSIONING.md에서 v0.1.0을 완료로 표시

금지:
- v0.2.0 이상 명령(PASS/NICK/USER 등) 구현 금지
- 바이너리 커밋 금지
```

---

## v0.2.0 프롬프트 — PASS/NICK/USER 등록 상태머신

```text
대상 버전: v0.2.0

목표:
- PASS/NICK/USER를 지원하고 "등록 완료" 조건을 고정한다.
- 등록 전 허용/거부 커맨드 목록을 고정한다.

반드시 선행(구현 전에):
- design/protocol/contract.md 갱신:
  - PASS/NICK/USER 문법, 성공/실패 numeric reply, 에러 코드
  - 등록 완료 조건(필드 필수성, 순서 허용 범위)
  - 등록 전 허용 커맨드(PING 등)와 거부 정책

테스트(필수):
- E2E:
  - PASS→NICK→USER 후 등록 성공
  - PASS 누락/오류 시 실패
  - 등록 전 JOIN/PRIVMSG 시 거부 확인

산출 문서:
- CLONE_GUIDE.md에 등록/접속 예시 추가
- design/server/v0.2.0-registration.md

완료 처리:
- VERSIONING.md v0.2.0 완료 표시
```

---

## v0.3.0 프롬프트 — 파서 고도화 + QUIT + PING/PONG + 에러 최소 세트

```text
대상 버전: v0.3.0

목표:
- IRC 메시지 파서를 RFC 스타일(프리픽스/트레일링 파라미터)로 고정한다.
- QUIT, PING/PONG을 지원한다.
- 최소 에러 세트(파라미터 부족, 알 수 없는 명령, 길이 초과 등)를 고정한다.

반드시 선행:
- contract.md 갱신:
  - 메시지 문법 요약
  - v0.3.0에서 사용하는 numeric replies 목록
  - 길이 초과 정책(드롭/에러/종료) 정확히 1개로 고정

테스트(필수):
- 단위:
  - prefix/params/trailing 파싱 케이스
- E2E:
  - QUIT 시 연결이 종료되는지
  - PING에 대한 PONG 형식
  - 잘못된 입력이 크래시 없이 에러 응답/종료되는지

산출 문서:
- design/server/v0.3.0-parser-errors.md
```

---

## v0.4.0 프롬프트 — JOIN/PART + 채널 브로드캐스트

```text
대상 버전: v0.4.0

목표:
- JOIN/PART를 구현하고 채널 멤버십을 관리한다.
- 채널 입장/퇴장/브로드캐스트 메시지를 최소한으로 고정한다.

반드시 선행:
- contract.md 갱신:
  - JOIN/PART 문법, 성공 시 서버가 보내는 메시지/숫자 응답
  - 채널 이름 규칙(최소)과 에러

테스트(필수):
- E2E:
  - 2클라 JOIN 같은 채널 → 서로의 JOIN 알림 수신
  - PART 후 채널 메시지 수신 금지

산출 문서:
- design/server/v0.4.0-channels.md
```

---

## v0.5.0 프롬프트 — PRIVMSG/NOTICE + NAMES/LIST

```text
대상 버전: v0.5.0

목표:
- PRIVMSG/NOTICE를 유저/채널 대상으로 라우팅한다.
- NAMES(채널 멤버 목록), LIST(채널 목록)를 제공한다.

반드시 선행:
- contract.md 갱신:
  - PRIVMSG/NOTICE 문법, 에러(대상 없음, 채널 미가입 등)
  - NAMES/LIST 응답 shape(사용 numeric)

테스트(필수):
- E2E:
  - 유저↔유저 메시지
  - 유저→채널 메시지 브로드캐스트
  - NAMES 결과에 두 유저가 포함

산출 문서:
- design/server/v0.5.0-messaging.md
```

---

## v0.6.0 프롬프트 — TOPIC/KICK/INVITE (최소 권한 모델)

```text
대상 버전: v0.6.0

목표:
- TOPIC 조회/설정
- KICK/INVITE 지원
- 권한 모델(최소 1개) 고정: 예) 채널 오퍼레이터만 KICK/INVITE 가능

반드시 선행:
- contract.md 갱신:
  - TOPIC/KICK/INVITE 문법
  - 권한 실패 시 에러 numeric

테스트(필수):
- E2E:
  - 권한 없는 사용자의 KICK 실패
  - INVITE 후 초대받은 사용자가 JOIN 성공

산출 문서:
- design/server/v0.6.0-channel-admin.md
```

---

## v0.7.0 프롬프트 — MODE(부분) +i +t +k +o +l

```text
대상 버전: v0.7.0

목표:
- MODE로 채널 모드(+i +t +k +o +l)를 적용/해제한다.
- 모드별 파라미터 규칙과 에러를 고정한다.

반드시 선행:
- contract.md 갱신:
  - MODE 문법(지원 모드만)
  - +k key, +l limit, +o nick 파라미터 규칙

테스트(필수):
- E2E:
  - +i에서 미초대 JOIN 실패
  - +k에서 잘못된 키 JOIN 실패
  - +l에서 인원 초과 JOIN 실패

산출 문서:
- design/server/v0.7.0-modes.md
```

---

## v0.8.0 프롬프트 — 설정/로깅/REHASH

```text
대상 버전: v0.8.0

목표:
- INI 설정 파일 로드(서버명/로그레벨/레이트리밋 등)
- 로깅 레벨 제어
- REHASH(또는 SIGHUP)로 설정 리로드(최소)

반드시 선행:
- contract.md 갱신:
  - 설정 파일 키 목록/기본값
  - REHASH 동작/제약

테스트(필수):
- 단위: 설정 파서
- 스모크: REHASH 후 값 반영(가능한 최소)

산출 문서:
- design/server/v0.8.0-config-logging.md
- CLONE_GUIDE.md에 설정 예시 추가
```

---

## v0.9.0 프롬프트 — Rate limiting + 백프레셔 정책 고정

```text
대상 버전: v0.9.0

목표:
- 메시지 플러딩을 완화하는 레이트리밋을 도입한다.
- outbound 큐 상한과 초과 정책(종료/드롭)을 계약+테스트로 고정한다.

반드시 선행:
- contract.md 갱신:
  - 레이트리밋 파라미터(윈도우/최대치)와 위반 시 정책
  - 백프레셔 초과 시 서버 행동(에러 후 종료 등)

테스트(필수):
- E2E:
  - 짧은 시간에 다량 PRIVMSG 전송 → 제한 발동
  - 느린 소비자 시나리오에서 서버 메모리 폭주 없이 정책 수행

산출 문서:
- design/server/v0.9.0-defensive.md
```

---

## v1.0.0 프롬프트 — 안정화 릴리즈(호환성/문서/테스트)

```text
대상 버전: v1.0.0

목표:
- 새 기능 추가 금지. 안정화/정리만 한다.
- contract.md를 동결 수준으로 정리하고 "지원/미지원"을 명확히 한다.
- CLONE_GUIDE.md를 그대로 따라하면 되는 수준으로 정리한다.
- 핵심 플로우 E2E 스모크를 보강한다.

필수 작업:
- 계약 정합성: contract.md ↔ 구현 ↔ 테스트 불일치 제거
- E2E 최소:
  - 등록(PASS/NICK/USER)
  - 채널 JOIN/PART
  - 채널 PRIVMSG 브로드캐스트
  - 모드 1개(+i 또는 +k) 정책 검증

산출 문서:
- design/server/v1.0.0-overview.md
- CLONE_GUIDE.md 최종 정리

완료 처리:
- VERSIONING.md v1.0.0 완료 표시
- known limitations 기록(3줄 이내)
```
