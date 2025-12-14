#!/usr/bin/env bash
# 설명: v0.1.0 기준 빌드 및 테스트 단일 진입점
# 버전: v0.1.0
# 관련 문서: CLONE_GUIDE.md

set -euo pipefail

make clean
make
make test
make e2e
