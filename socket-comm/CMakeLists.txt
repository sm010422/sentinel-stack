cmake_minimum_required(VERSION 3.20)

# ─────────────────────────────────────────────────────────────────────────────
# Layer 2: 소켓 통신 빌드 설정
#
# 생성되는 타겟:
#   socket_comm   — 정적 라이브러리
#   comm_node     — 통신 노드 실행 파일 (commander / sensor / relay)
#   latency_test  — RTT 벤치마크
#   test_reconnect — 재연결 테스트
# ─────────────────────────────────────────────────────────────────────────────

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# ── 정적 라이브러리 ───────────────────────────────────────────────────────────
add_library(socket_comm STATIC
    src/protocol.c
    src/crypto.c
    src/tcp_transport.c
    src/udp_transport.c
    src/pubsub.c
    src/comm_node.c
    # 플랫폼별 I/O 멀티플렉서
    $<$<PLATFORM_ID:Linux>:src/io_epoll.c>
    $<$<PLATFORM_ID:Darwin>:src/io_kqueue.c>
)

target_include_directories(socket_comm PUBLIC include)

target_link_libraries(socket_comm PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(socket_comm PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# ── comm_node 실행 파일 ───────────────────────────────────────────────────────
add_executable(comm_node
    src/protocol.c
    src/crypto.c
    src/tcp_transport.c
    src/udp_transport.c
    src/pubsub.c
    src/comm_node.c
    $<$<PLATFORM_ID:Linux>:src/io_epoll.c>
    $<$<PLATFORM_ID:Darwin>:src/io_kqueue.c>
)

target_include_directories(comm_node PRIVATE include)
target_compile_definitions(comm_node PRIVATE COMM_STANDALONE)
target_link_libraries(comm_node PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
target_compile_options(comm_node PRIVATE -Wall -Wextra)

# ── latency_test ─────────────────────────────────────────────────────────────
add_executable(latency_test
    tests/test_latency.c
    src/protocol.c
    src/crypto.c
    src/tcp_transport.c
    src/udp_transport.c
    src/pubsub.c
    $<$<PLATFORM_ID:Linux>:src/io_epoll.c>
    $<$<PLATFORM_ID:Darwin>:src/io_kqueue.c>
)

target_include_directories(latency_test PRIVATE include)
target_link_libraries(latency_test PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

# ── test_reconnect ────────────────────────────────────────────────────────────
add_executable(test_reconnect
    tests/test_reconnect.c
    src/protocol.c
    src/crypto.c
    src/tcp_transport.c
    src/udp_transport.c
    src/pubsub.c
    src/comm_node.c
    $<$<PLATFORM_ID:Linux>:src/io_epoll.c>
    $<$<PLATFORM_ID:Darwin>:src/io_kqueue.c>
)

target_include_directories(test_reconnect PRIVATE include)
target_link_libraries(test_reconnect PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

# CTest 등록
add_test(NAME comm_latency   COMMAND latency_test)
add_test(NAME comm_reconnect COMMAND test_reconnect)
