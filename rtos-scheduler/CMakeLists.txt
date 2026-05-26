cmake_minimum_required(VERSION 3.20)

# ─────────────────────────────────────────────────────────────────────────────
# Layer 1: RTOS 스케줄러 빌드 설정
#
# 생성되는 타겟:
#   rtos_scheduler   — 정적 라이브러리 (다른 모듈에서 링크 가능)
#   rtos_sim         — 독립 실행 파일 (CLI 시뮬레이터)
#   benchmark_jitter — 지터 측정 벤치마크
#   test_rtos        — 단위 테스트
# ─────────────────────────────────────────────────────────────────────────────

# ── 정적 라이브러리 ───────────────────────────────────────────────────────────
add_library(rtos_scheduler STATIC
    src/timer.c
    src/rms.c
    src/edf.c
    src/scheduler.c
)

target_include_directories(rtos_scheduler PUBLIC include)

# 수학 함수 (pow, log — RMS 사용률 한계 계산)
target_link_libraries(rtos_scheduler PUBLIC m)

target_compile_options(rtos_scheduler PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# ── rtos_sim 실행 파일 ────────────────────────────────────────────────────────
add_executable(rtos_sim
    src/scheduler.c
    src/timer.c
    src/rms.c
    src/edf.c
)

target_include_directories(rtos_sim PRIVATE include)
target_link_libraries(rtos_sim PRIVATE m)

# RTOS_STANDALONE: main() 활성화
target_compile_definitions(rtos_sim PRIVATE RTOS_STANDALONE)

target_compile_options(rtos_sim PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# ── benchmark_jitter ─────────────────────────────────────────────────────────
add_executable(benchmark_jitter
    tests/test_jitter.c
    src/scheduler.c
    src/timer.c
    src/rms.c
    src/edf.c
)

target_include_directories(benchmark_jitter PRIVATE include)
target_link_libraries(benchmark_jitter PRIVATE m)

target_compile_options(benchmark_jitter PRIVATE -Wall -Wextra -O2)

# ── 단위 테스트 ──────────────────────────────────────────────────────────────
add_executable(test_rtos
    tests/test_scheduler.c
    src/scheduler.c
    src/timer.c
    src/rms.c
    src/edf.c
)

target_include_directories(test_rtos PRIVATE include)
target_link_libraries(test_rtos PRIVATE m)
target_compile_definitions(test_rtos PRIVATE BUILD_TESTS)

target_compile_options(test_rtos PRIVATE -Wall -Wextra -Wpedantic)

# CTest 등록
add_test(NAME rtos_unit    COMMAND test_rtos)
add_test(NAME rtos_jitter  COMMAND benchmark_jitter)
