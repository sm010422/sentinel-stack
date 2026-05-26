#!/usr/bin/env bash
# =============================================================================
# scenario_01.sh — SYN Flood 탐지 시나리오
#
# sentinel-stack 이상 탐지 시연 시나리오입니다.
# Layer 2 통신 + Layer 3 실시간 탐지를 함께 검증합니다.
#
# 시나리오:
#   1. Commander/Sensor 노드 정상 통신 (30초)
#   2. SYN Flood 시뮬레이션 (hping3 또는 nmap)
#   3. sentinel_pcap 이 SYN Flood 경보 발생
#   4. 결과 CSV 저장
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"
OUTPUT_DIR="$ROOT_DIR/output"

mkdir -p "$OUTPUT_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_CSV="$OUTPUT_DIR/scenario_01_${TIMESTAMP}.csv"

echo "======================================================"
echo "  sentinel-stack 시나리오 01: SYN Flood 탐지"
echo "======================================================"

# ── 1단계: 패킷 분석기 백그라운드 시작 ────────────────────────────────────
IFACE="lo0"
if [[ "$(uname)" == "Linux" ]]; then IFACE="lo"; fi

echo "[1/4] 패킷 분석기 시작 (인터페이스: $IFACE)"
sudo "$BUILD_DIR/packet-analyzer/sentinel_pcap" \
    -i "$IFACE" \
    --report "$REPORT_CSV" &

PCAP_PID=$!
sleep 2

# ── 2단계: Commander 노드 시작 ──────────────────────────────────────────────
echo "[2/4] Commander 노드 시작"
"$BUILD_DIR/socket-comm/comm_node" \
    --id 1 --role commander \
    --tcp-port 9000 --udp-port 9001 &

COMMANDER_PID=$!
sleep 1

# ── 3단계: Sensor 노드 연결 ─────────────────────────────────────────────────
echo "[3/4] Sensor 노드 연결"
"$BUILD_DIR/socket-comm/comm_node" \
    --id 2 --role sensor \
    --connect 127.0.0.1:9000 &

SENSOR_PID=$!
sleep 5

# ── 4단계: SYN Flood 시뮬레이션 ─────────────────────────────────────────────
echo "[4/4] SYN Flood 시뮬레이션..."
echo "     (hping3 없을 경우 nmap -sS 사용)"

if command -v hping3 &>/dev/null; then
    # hping3: 100 SYN 패킷 전송
    sudo hping3 --syn --count 100 --faster 127.0.0.1 -p 9000 2>/dev/null || true
elif command -v nmap &>/dev/null; then
    # nmap SYN 스캔
    sudo nmap -sS --min-rate 100 -p 9000 127.0.0.1 2>/dev/null || true
else
    echo "     hping3/nmap 없음 — 수동 시나리오로 진행"
fi

sleep 10

# ── 종료 ────────────────────────────────────────────────────────────────────
echo ""
echo "[종료] 모든 프로세스 중단"
kill "$PCAP_PID" "$COMMANDER_PID" "$SENSOR_PID" 2>/dev/null || true
wait 2>/dev/null || true

echo ""
echo "======================================================"
echo "  시나리오 완료"
echo "  CSV 리포트: $REPORT_CSV"
echo "======================================================"
