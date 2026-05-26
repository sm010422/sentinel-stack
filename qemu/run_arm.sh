#!/usr/bin/env bash
# =============================================================================
# run_arm.sh — QEMU ARM Cortex-M3 에뮬레이션 실행
#
# mps2-an385 보드 (ARM Cortex-M3)에서 RTOS 스케줄러를 실행합니다.
# 방산 임베디드 시스템의 실제 하드웨어 타겟 환경을 시뮬레이션합니다.
#
# 사전 요건:
#   macOS: brew install qemu
#   Linux: sudo apt-get install qemu-system-arm
#   크로스 컴파일러: arm-none-eabi-gcc
#
# 빌드 (크로스 컴파일):
#   cmake -B build-arm -G Ninja \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DBUILD_FOR_QEMU=ON
#   cmake --build build-arm
#
# 실행:
#   ./qemu/run_arm.sh
#   ./qemu/run_arm.sh --debug   # GDB 서버 포함 (포트 1234)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
ELF="$ROOT_DIR/build-arm/rtos-scheduler/rtos_qemu.elf"

# ── 바이너리 확인 ────────────────────────────────────────────────────────────
if [[ ! -f "$ELF" ]]; then
    echo "[ERROR] QEMU 빌드 바이너리 없음: $ELF"
    echo ""
    echo "크로스 컴파일 방법:"
    echo "  cmake -B build-arm -G Ninja -DBUILD_FOR_QEMU=ON"
    echo "  cmake --build build-arm"
    exit 1
fi

# ── QEMU 확인 ────────────────────────────────────────────────────────────────
if ! command -v qemu-system-arm &>/dev/null; then
    echo "[ERROR] qemu-system-arm 없음"
    echo "  macOS: brew install qemu"
    echo "  Linux: sudo apt-get install qemu-system-arm"
    exit 1
fi

echo "======================================================"
echo "  sentinel-stack QEMU ARM Cortex-M3 에뮬레이션"
echo "======================================================"
echo "  타겟: ARM Cortex-M3 (mps2-an385)"
echo "  ELF:  $ELF"
echo "  종료: Ctrl-A X"
echo "======================================================"
echo ""

# ── GDB 디버그 모드 ─────────────────────────────────────────────────────────
if [[ "${1:-}" == "--debug" ]]; then
    echo "[DEBUG] GDB 서버 포트 1234 대기 중..."
    echo "        연결: arm-none-eabi-gdb $ELF -ex 'target remote :1234'"
    QEMU_EXTRA="-gdb tcp::1234 -S"
else
    QEMU_EXTRA=""
fi

# ── QEMU 실행 ───────────────────────────────────────────────────────────────
qemu-system-arm \
    -machine mps2-an385 \
    -cpu cortex-m3 \
    -kernel "$ELF" \
    -nographic \
    -serial stdio \
    -monitor none \
    ${QEMU_EXTRA:-}
