cmake_minimum_required(VERSION 3.20)

project(sentinel-stack
    VERSION     1.0.0
    DESCRIPTION "전술 통신 미들웨어 스택 — 방산 임베디드 시스템 포트폴리오"
    LANGUAGES   C
)

# ─────────────────────────────────────────────────────────────────────────────
# 전역 빌드 설정
# ─────────────────────────────────────────────────────────────────────────────

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF) # POSIX 확장이 필요한 경우 ON

# 기본 빌드 타입
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "빌드 타입: ${CMAKE_BUILD_TYPE}")
message(STATUS "플랫폼: ${CMAKE_SYSTEM_NAME}")
message(STATUS "아키텍처: ${CMAKE_SYSTEM_PROCESSOR}")

# ─────────────────────────────────────────────────────────────────────────────
# 전역 컴파일 플래그
# ─────────────────────────────────────────────────────────────────────────────

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic

    # MISRA-C 관련 경고 강화
    -Wshadow
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wcast-align

    # 보안 강화
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# ARM 크로스 컴파일 설정 (QEMU 타겟)
# ─────────────────────────────────────────────────────────────────────────────

option(BUILD_FOR_QEMU "ARM Cortex-M3 (QEMU mps2-an385) 용 크로스 컴파일" OFF)

if(BUILD_FOR_QEMU)
    message(STATUS "QEMU ARM Cortex-M3 빌드 활성화")
    set(CMAKE_SYSTEM_NAME       Generic)
    set(CMAKE_SYSTEM_PROCESSOR  arm)
    set(CMAKE_C_COMPILER        arm-none-eabi-gcc)

    add_compile_options(
        -mcpu=cortex-m3
        -mthumb
        -mfloat-abi=soft
        -nostdlib
        -ffreestanding
        -fno-exceptions
    )

    add_link_options(
        -T${CMAKE_SOURCE_DIR}/qemu/linker.ld
        -nostdlib
        -Wl,--gc-sections
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 모듈별 서브디렉토리
# ─────────────────────────────────────────────────────────────────────────────

# BUILD_MODULE 옵션으로 특정 모듈만 빌드 가능
set(BUILD_MODULE "all" CACHE STRING "빌드할 모듈 (all|rtos|comm|pcap)")

enable_testing()

if(BUILD_MODULE STREQUAL "all" OR BUILD_MODULE STREQUAL "rtos")
    message(STATUS "Layer 1: RTOS 스케줄러 빌드")
    add_subdirectory(rtos-scheduler)
endif()

if(BUILD_MODULE STREQUAL "all" OR BUILD_MODULE STREQUAL "comm")
    if(NOT BUILD_FOR_QEMU)
        message(STATUS "Layer 2: 소켓 통신 빌드")
        add_subdirectory(socket-comm)
    else()
        message(STATUS "Layer 2: QEMU 빌드에서 소켓 통신 제외")
    endif()
endif()

if(BUILD_MODULE STREQUAL "all" OR BUILD_MODULE STREQUAL "pcap")
    if(NOT BUILD_FOR_QEMU)
        message(STATUS "Layer 3: 패킷 분석기 빌드")
        add_subdirectory(packet-analyzer)
    else()
        message(STATUS "Layer 3: QEMU 빌드에서 패킷 분석기 제외")
    endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
# 요약 출력
# ─────────────────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "════════════════════════════════════════")
message(STATUS "  sentinel-stack 빌드 설정 요약")
message(STATUS "════════════════════════════════════════")
message(STATUS "  버전       : ${PROJECT_VERSION}")
message(STATUS "  빌드 타입  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  모듈       : ${BUILD_MODULE}")
message(STATUS "  QEMU 크로스: ${BUILD_FOR_QEMU}")
message(STATUS "════════════════════════════════════════")
message(STATUS "")
