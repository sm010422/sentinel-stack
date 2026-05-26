cmake_minimum_required(VERSION 3.20)

# ─────────────────────────────────────────────────────────────────────────────
# Layer 3: 패킷 분석기 빌드 설정
# ─────────────────────────────────────────────────────────────────────────────

find_library(PCAP_LIBRARIES NAMES pcap REQUIRED)
find_path(PCAP_INCLUDE_DIRS NAMES pcap.h PATHS
    /usr/include
    /usr/local/include
    /opt/homebrew/include
    REQUIRED)
if(NOT PCAP_LIBRARIES OR NOT PCAP_INCLUDE_DIRS)
    message(FATAL_ERROR "libpcap not found. macOS: brew install libpcap, Linux: apt install libpcap-dev")
endif()
message(STATUS "libpcap: ${PCAP_LIBRARIES}")
message(STATUS "libpcap headers: ${PCAP_INCLUDE_DIRS}")
find_package(Curses)

# ── 정적 라이브러리 ───────────────────────────────────────────────────────────
add_library(packet_analyzer STATIC
    src/capture.c
    src/parser_eth.c
    src/parser_ip.c
    src/parser_tcp.c
    src/parser_custom.c
    src/rule_engine.c
    src/reporter.c
)

target_include_directories(packet_analyzer PUBLIC include ${PCAP_INCLUDE_DIRS})
target_link_libraries(packet_analyzer PUBLIC ${PCAP_LIBRARIES})

if(CURSES_FOUND)
    target_compile_definitions(packet_analyzer PRIVATE HAVE_NCURSES)
    target_link_libraries(packet_analyzer PUBLIC ${CURSES_LIBRARIES})
endif()

target_compile_options(packet_analyzer PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ── sentinel_pcap 실행 파일 ───────────────────────────────────────────────────
add_executable(sentinel_pcap
    src/capture.c
    src/parser_eth.c
    src/parser_ip.c
    src/parser_tcp.c
    src/parser_custom.c
    src/rule_engine.c
    src/reporter.c
)

target_include_directories(sentinel_pcap PRIVATE include ${PCAP_INCLUDE_DIRS})
target_compile_definitions(sentinel_pcap PRIVATE PCAP_STANDALONE)
target_link_libraries(sentinel_pcap PRIVATE ${PCAP_LIBRARIES})

if(CURSES_FOUND)
    target_link_libraries(sentinel_pcap PRIVATE ${CURSES_LIBRARIES})
endif()

target_compile_options(sentinel_pcap PRIVATE -Wall -Wextra)

# ── 단위 테스트 ──────────────────────────────────────────────────────────────
add_executable(test_parser
    tests/test_parser.c
    src/parser_eth.c
    src/parser_ip.c
    src/parser_tcp.c
    src/parser_custom.c
    src/rule_engine.c
    src/reporter.c
)

target_include_directories(test_parser PRIVATE include ${PCAP_INCLUDE_DIRS})
target_link_libraries(test_parser PRIVATE ${PCAP_LIBRARIES})
target_compile_options(test_parser PRIVATE -Wall -Wextra)

add_test(NAME pcap_parser COMMAND test_parser)
