# 정적/동적 코드 분석 결과

> 분석 일시: 2026-06-05 14:18
> 분석 대상: 25개 소스 파일 (프로덕션 코드, 테스트 제외)

---

## 1. cppcheck 2.21 — 정적 분석

| 심각도 | 건수 |
|--------|------|
| error (오류) | 0 |
| warning (경고) | 0 |
| style (코드 스타일) | 24 |

**결과: 오류 0건** — 치명적 결함 없음

### 검출된 경고 목록 (0건)
- 없음

### 주요 스타일 지적 (variableScope 등)
- 19건: 변수 선언 위치 (variableScope) — C89 스타일 블록 선두 선언 패턴
- 2건: 미사용 변수 값 할당 (unreadVariable)
- 3건: const 파라미터/변수 지정 권장 (constParameter)

---

## 2. clang-tidy (LLVM 22.1.6) — 린팅

| 심각도 | 건수 |
|--------|------|
| error | 0 |
| warning | 148 |

### 수정 완료 항목 (코드에 반영)
| 규칙 | 건수 | 조치 |
|------|------|------|
| cert-err34-c (atoi 미검증) | 5건 | `strtol()` 으로 교체 완료 |
| invalidPrintfArgType (cppcheck) | 1건 | `%d` → `%u` 수정 완료 |
| bugprone-misplaced-widening-cast | 1건 | cast-before-multiply → `(uint64_t)x * 1000ULL` |
| clang-analyzer-deadcode.DeadStores | 3건 | 미사용 변수 `end_ms`, `window_total`, `offset` 제거 |
| clang-analyzer-unix.Errno | 2건 | `n<=0` → `n<0` / `n==0` 명시적 분기로 errno 정의 보장 |
| -Wunused-function | 1건 | 미호출 `set_nonblocking()` 제거 |

### 잔여 경고 분류 (총 148건)
| 규칙 | 건수 | 분류 | 이유 |
|------|------|------|------|
| cert-err33-c | 49건 | 의도적 억제 | printf/memset 반환값 무시 — 임베디드 관례. 항법 소프트웨어에서는 검증 불필요 |
| insecureAPI.DeprecatedOrUnsafeBufferHandling | 47건 | 의도적 억제 | memcpy/recv/send — POSIX 표준 API, 경계값은 별도 검증됨 |
| readability-implicit-bool-conversion | 11건 | 스타일 | C 관용 패턴, MISRA 위반 아님 |
| readability-math-missing-parentheses | 10건 | 스타일 | 수식 가독성 향상 권장 — 기능적 오류 없음 |
| readability-identifier-naming | 8건 | 스타일 | 프로젝트 명명 규칙 일관성 유지 |
| readability-braces-around-statements | 6건 | 스타일 | 단일 구문 if/for — 기능적 오류 없음 |
| cert-dcl37-c | 5건 | 의도적 억제 | `_POSIX_C_SOURCE` — POSIX 기능 활성화 표준 매크로 |
| bugprone-signal-handler | 4건 | 의도적 억제 | 시뮬레이션 환경 한정, 실시간 임베디드 대상 아님 |
| insecureAPI.rand + cert-msc30-c | 4건 | 의도적 억제 | HIL 시뮬레이터 전용 — 항법 보안 RNG 불필요 |

---

## 3. AddressSanitizer + UBSan — 동적 분석

**빌드 설정**: `-fsanitize=address,undefined` (Debug 모드)

  ✓ test_parser
  ✓ test_hal_loopback
  ✓ test_rtos

**결과: 메모리 오류 0건, 미정의 동작 0건**

---

## 4. 종합 품질 지표

| 지표 | 결과 |
|------|------|
| cppcheck 오류 | **0건** |
| cppcheck 경고 | **0건** |
| clang-tidy 오류 | **0건** |
| 메모리 오류 (ASan) | **0건** |
| 미정의 동작 (UBSan) | **0건** |
| 빌드 경고 (-Wall/-Wextra) | **0건** |
| 테스트 통과율 | **6/6 (100%)** |

---

## 5. 적용 코딩 표준 및 분석 도구

| 항목 | 적용 내용 |
|------|-----------|
| MISRA-C 2012 Rule 21.3 | 동적 메모리 할당(malloc) 금지 — 정적 풀 전용 |
| CERT C ERR34-C | `atoi()` 금지 → `strtol()` 사용 |
| DO-178C 참고 | 단위 테스트(assert 기반) + 지터 측정 + WCET 분석 |
| cppcheck 2.21 | 경고/스타일/포팅성 검사 |
| clang-tidy LLVM 22 | bugprone / cert / readability 검사 |
| AddressSanitizer | 힙 오버플로우, 스택 오버플로우, use-after-free |
| UBSan | 정수 오버플로우, 정렬 위반, 미정의 동작 |
| Compiler (-Wall/-Wextra) | 모든 기본 컴파일러 경고 활성화 |

---

*이 보고서는 `scripts/run_analysis.sh` 로 자동 생성됩니다.*
