# VERSIONING.md

modern-irc (C++17 IRC 서버) 버전 로드맵.

원칙:
- 한 번의 변경 세트(커밋/PR)는 **정확히 1개 버전**만 타겟으로 한다.
- 외부 동작(지원 명령/응답/정책)이 바뀌면 **구현 전에** `design/protocol/contract.md`부터 갱신한다.
- 바이너리(이미지/로그/빌드 산출물 등)는 커밋 금지.

---

## 상태 표기
- ⬜ 계획(Planned)
- 🟨 진행중(In progress)
- ✅ 완료(Done)

---

## 버전 로드맵 (0.1.0 단위)

### v0.1.0 — 스켈레톤 + 계약 고정 + TCP 수락 + E2E 스모크
- 상태: ⬜
- 목표:
  - 기본 폴더 구조(include/src/tests/design/tools)
  - 서버가 포트를 열고 다중 접속을 `poll()`로 처리
  - `design/protocol/contract.md` 생성(최소 규약: 라인 프레이밍/CRLF, 에러 엔벨로프(해당 시), 지원 최소 커맨드/이벤트)
  - E2E: TCP 연결 후 최소 1개 라인 왕복(예: 서버 PING 응답 또는 임시 ECHO)
- 필수 테스트:
  - 단위 1개(예: 메시지 프레이밍 파서)
  - E2E 1개(실제 소켓)

### v0.2.0 — 등록(Registration) 1차: PASS/NICK/USER
- 상태: ⬜
- 목표:
  - PASS/NICK/USER 지원
  - 등록 완료 조건 고정(순서/필수값)
  - 등록 전 커맨드 제한 정책 고정
- 필수 테스트:
  - 등록 성공/실패 케이스 E2E

### v0.3.0 — 프로토콜 기반: 파싱/에러/QUIT/PING-PONG
- 상태: ⬜
- 목표:
  - RFC 스타일 메시지 파싱(프리픽스/트레일링 파라미터)
  - PING/PONG, QUIT
  - 핵심 에러 코드(최소 세트)와 메시지 형태 고정
- 필수 테스트:
  - 잘못된 입력(길이 초과/파라미터 부족) 처리 E2E

### v0.4.0 — 채널 기초: JOIN/PART + 브로드캐스트
- 상태: ⬜
- 목표:
  - JOIN/PART
  - 채널 멤버십/브로드캐스트(입장/퇴장 메시지 포함)
- 필수 테스트:
  - 2클라: JOIN → 서로에게 알림
  - PART 후 라우팅 차단

### v0.5.0 — 메시징: PRIVMSG/NOTICE + NAMES/LIST
- 상태: ⬜
- 목표:
  - 사용자↔사용자, 사용자→채널 PRIVMSG/NOTICE
  - NAMES(채널 유저 목록), LIST(채널 목록)
  - 기본 numeric replies 정리
- 필수 테스트:
  - 2~3클라 메시지 라우팅 E2E

### v0.6.0 — 채널 관리: TOPIC/KICK/INVITE
- 상태: ⬜
- 목표:
  - TOPIC 설정/조회
  - KICK/INVITE (권한 정책 최소 1개로 고정)
- 필수 테스트:
  - 권한 없는 KICK 실패
  - INVITE 플로우(초대 후 JOIN 가능)

### v0.7.0 — MODE(부분): +i +t +k +o +l
- 상태: ⬜
- 목표:
  - 채널 모드 적용/해제
  - 모드별 파라미터 처리(+k key, +l limit, +o nick)
- 필수 테스트:
  - invite-only(+i)에서 미초대 JOIN 실패
  - key(+k) 불일치 JOIN 실패

### v0.8.0 — 운영 편의: 설정 파일 + 로깅 레벨 + REHASH
- 상태: ⬜
- 목표:
  - INI 스타일 설정 파일 로드
  - 로깅 레벨/파일 경로
  - REHASH(또는 SIGHUP)로 런타임 리로드(최소)
- 필수 테스트:
  - 설정 로드 단위 테스트
  - REHASH 후 특정 값 반영 스모크

### v0.9.0 — 방어: Rate limiting + 백프레셔 정책 고정
- 상태: ⬜
- 목표:
  - per-connection 메시지 레이트리밋
  - outbound 큐 제한/초과 정책(드롭/종료) 고정
- 필수 테스트:
  - 스팸 전송 시 제한 발동
  - 느린 소비자 시나리오에서 메모리 폭주 방지

### v1.0.0 — 안정화: 호환성 점검 + 문서/테스트 정리
- 상태: ⬜
- 목표:
  - README의 명령어 레퍼런스 범위 내에서 **지원/미지원** 명확화
  - contract.md/테스트/구현 정합성 동결 수준으로 정리
  - CLONE_GUIDE.md를 “그대로 따라하면 된다” 수준으로 보강
- 필수 테스트:
  - E2E 스모크(health/handshake/채널/메시지/모드 1개)

---

## Known limitations (기록)
- (비워둠) 각 버전 완료 시 여기에 "현재 미지원"을 짧게 기록한다.
