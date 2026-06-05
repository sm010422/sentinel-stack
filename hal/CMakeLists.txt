cmake_minimum_required(VERSION 3.20)

# ── HAL 정적 라이브러리 ────────────────────────────────────────────────────
add_library(sentinel_hal STATIC
    src/hal_uart_posix.c
    src/hal_i2c_posix.c
    src/hal_spi_posix.c
    src/hal_can_posix.c
)

target_include_directories(sentinel_hal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(sentinel_hal PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes
)

# ── 루프백 단위 테스트 ─────────────────────────────────────────────────────
add_executable(test_hal_loopback tests/test_hal_loopback.c)

target_link_libraries(test_hal_loopback PRIVATE sentinel_hal)

add_test(
    NAME    hal_loopback
    COMMAND test_hal_loopback
)

message(STATUS "  HAL 라이브러리: UART / RS-422 / I2C / SPI / CAN")
