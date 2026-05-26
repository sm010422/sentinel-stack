#!/usr/bin/env bash
# =============================================================================
# run_all.sh — sentinel-stack 통합 데모 실행 스크립트
#
# tmux 4분할 터미널에서 세 레이어를 동시 실행합니다:
#   좌상: RTOS 스케줄러 시뮬레이션
#   우상: Commander 노드 (지휘소)
#   좌하: Sensor 노드 (센서)
#   우하: 패킷 분석기 (lo0 모니터링)
#
# 사전 요건:
#   - cmake --build build 완료
#   - brew install tmux (macOS)
#   - sudo 권한 (패킷 캡처용)
#
# 사용법:
#   ./demo/run_all.sh
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"

# ── 빌드 확인 ───────────────────────────────────────────────────────────────
check_binary() {
    local bin="$1"
    if [[ ! -f "$bin" ]]; then
        echo "[ERROR] 바이너리 없음: $bin"
        echo "        먼저 빌드하세요: cmake --build $BUILD_DIR"
        exit 1
    fi
}

check_binary "$BUILD_DIR/rtos-scheduler/rtos_sim"
check_binary "$BUILD_DIR/socket-comm/comm_node"
check_binary "$BUILD_DIR/packet-analyzer/sentinel_pcap"

# ── tmux 확인 ───────────────────────────────────────────────────────────────
if ! command -v tmux &>/dev/null; then
    echo "[ERROR] tmux가 설치되어 있지 않습니다."
    echo "        macOS: brew install tmux"
    exit 1
fi

SESSION="sentinel-demo"

# 기존 세션 종료
tmux kill-session -t "$SESSION" 2>/dev/null || true

echo "======================================================"
echo "  sentinel-stack 통합 데모 시작"
echo "======================================================"
echo "  종료: Ctrl-C 또는 tmux kill-session -t $SESSION"
echo ""

# ── tmux 세션 생성 ──────────────────────────────────────────────────────────
tmux new-session -d -s "$SESSION" -x 220 -y 50

# 4분할 레이아웃
tmux split-window -h -t "$SESSION:0"   # 좌우 분할
tmux split-window -v -t "$SESSION:0.0" # 좌측 상하 분할
tmux split-window -v -t "$SESSION:0.2" # 우측 상하 분할

# ── 좌상: RTOS 스케줄러 ─────────────────────────────────────────────────────
tmux send-keys -t "$SESSION:0.0" \
    "echo '=== Layer 1: RTOS Scheduler (RMS) ===' && \
     sleep 1 && \
     $BUILD_DIR/rtos-scheduler/rtos_sim --algo rms --tasks 5 --duration 60s" \
    Enter

# ── 우상: Commander 노드 ─────────────────────────────────────────────────────
tmux send-keys -t "$SESSION:0.2" \
    "echo '=== Layer 2: Commander Node (id=1) ===' && \
     sleep 2 && \
     $BUILD_DIR/socket-comm/comm_node --id 1 --role commander \
         --tcp-port 9000 --udp-port 9001" \
    Enter

# ── 좌하: Sensor 노드 ───────────────────────────────────────────────────────
tmux send-keys -t "$SESSION:0.1" \
    "echo '=== Layer 2: Sensor Node (id=2) ===' && \
     sleep 3 && \
     $BUILD_DIR/socket-comm/comm_node --id 2 --role sensor \
         --connect 127.0.0.1:9000 \
         --subscribe SENSOR_DATA,STATUS" \
    Enter

# ── 우하: 패킷 분석기 ───────────────────────────────────────────────────────
IFACE="lo0"
if [[ "$(uname)" == "Linux" ]]; then
    IFACE="lo"
fi

tmux send-keys -t "$SESSION:0.3" \
    "echo '=== Layer 3: Packet Analyzer (${IFACE}) ===' && \
     sleep 4 && \
     sudo $BUILD_DIR/packet-analyzer/sentinel_pcap -i ${IFACE}" \
    Enter

# ── 레이아웃 정렬 ───────────────────────────────────────────────────────────
tmux select-layout -t "$SESSION:0" tiled

echo "tmux 세션 '$SESSION' 시작됨"
echo "연결: tmux attach-session -t $SESSION"
echo ""

# 포어그라운드로 연결
tmux attach-session -t "$SESSION"
