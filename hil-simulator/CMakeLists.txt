cmake_minimum_required(VERSION 3.20)

# ── IMU 시뮬레이터 라이브러리 ─────────────────────────────────────────────
add_library(sentinel_imu STATIC
    src/imu_sim.c
)

target_include_directories(sentinel_imu
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/hal/include
)

target_link_libraries(sentinel_imu PRIVATE m)  # libm (sqrtf, sinf, cosf)

# ── HIL 데모 실행 파일 ────────────────────────────────────────────────────
add_executable(hil_demo src/hil_demo.c)

target_link_libraries(hil_demo PRIVATE
    sentinel_imu
    sentinel_hal
    m
)

target_include_directories(hil_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/hal/include
)

message(STATUS "  HIL 시뮬레이터: 6-DOF IMU + I2C + CAN 버스 데모")
