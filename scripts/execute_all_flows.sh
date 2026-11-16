#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGS_DIR="$ROOT_DIR/logs"
DATASET_DIR="/mnt/d/College/SEM 5/IT 305 Computer Networks/Lab06 - Socket Programming/dataset/raw-img"
TMP_DIR="/tmp/cn_flows_$$"

mkdir -p "$LOGS_DIR"
mkdir -p "$TMP_DIR"

cleanup() {
    kill -9 $(jobs -p) 2>/dev/null || true
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# Topic demo files
mkdir -p "$TMP_DIR/dataset/topic_demo"
for i in {1..12}; do
    head -c $((32768 * i)) /dev/urandom > "$TMP_DIR/dataset/topic_demo/asset_${i}.bin"
done
mkdir -p "$TMP_DIR/dataset/telemetry"
echo "Telemetry stream content" > "$TMP_DIR/dataset/telemetry/stream.log"
DEMO_DATASET="$TMP_DIR/dataset"

# Flow 1: Broker / Server Startup
PORT_1=19101
cd "$ROOT_DIR/Part1"
stdbuf -oL -eL ./server $PORT_1 "$DATASET_DIR" multi > "$LOGS_DIR/broker_startup.log" 2>&1 &
PID_1=$!
sleep 1
echo "" >> "$LOGS_DIR/broker_startup.log"
echo "Active Listening Port State:" >> "$LOGS_DIR/broker_startup.log"
ss -tulpn | grep "$PORT_1" >> "$LOGS_DIR/broker_startup.log" 2>&1 || true
kill -9 $PID_1 2>/dev/null || true
wait $PID_1 2>/dev/null || true

# Flow 2: Part 1 Transfer
PORT_2=19102
cd "$ROOT_DIR/Part1"
stdbuf -oL -eL ./server $PORT_2 "$DEMO_DATASET" multi > "$LOGS_DIR/part1_server.log" 2>&1 &
PID_2=$!
sleep 1
mkdir -p "$TMP_DIR/out_p1"
./client 127.0.0.1 $PORT_2 topic_demo "$TMP_DIR/out_p1" > "$LOGS_DIR/part1_client.log" 2>&1
kill -9 $PID_2 2>/dev/null || true
wait $PID_2 2>/dev/null || true

# Flow 5: Part 2 Case 2 Enhanced Parallel Streaming
PORT_5=19105
cd "$ROOT_DIR/Part2_Case2_Enhanced"
stdbuf -oL -eL ./server $PORT_5 "$DEMO_DATASET" 0.08 > "$LOGS_DIR/part2_enh_server.log" 2>&1 &
PID_5=$!
sleep 1
mkdir -p "$TMP_DIR/out_p2enh"
./client 127.0.0.1 $PORT_5 topic_demo "$TMP_DIR/out_p2enh" 4 > "$LOGS_DIR/part2_enh_client.log" 2>&1
kill -9 $PID_5 2>/dev/null || true
wait $PID_5 2>/dev/null || true

echo "Flow logs 1, 2, 5 refreshed successfully."
