#!/bin/bash

# Ensure output directories exist
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RESULTS_DIR="$ROOT_DIR/Results"
mkdir -p "$RESULTS_DIR"

# Dynamic dataset path resolution
if [ -n "$DATASET_PATH" ] && [ -d "$DATASET_PATH" ]; then
    DATASET_DIR="$(cd "$DATASET_PATH" && pwd)"
elif [ -d "$ROOT_DIR/../../../dataset/raw-img" ]; then
    DATASET_DIR="$(cd "$ROOT_DIR/../../../dataset/raw-img" && pwd)"
elif [ -d "$ROOT_DIR/../../dataset/raw-img" ]; then
    DATASET_DIR="$(cd "$ROOT_DIR/../../dataset/raw-img" && pwd)"
elif [ -d "$ROOT_DIR/dataset/raw-img" ]; then
    DATASET_DIR="$(cd "$ROOT_DIR/dataset/raw-img" && pwd)"
else
    DATASET_DIR="$ROOT_DIR/dataset"
fi

TOPICS=${BENCHMARK_TOPICS:-"cane cavallo elefante farfalla gallina gatto mucca pecora ragno scoiattolo"}
MODE=${1:-"all"}

echo "Using dataset directory: $DATASET_DIR"
echo "Benchmark mode: $MODE"

# ------------------------------------------------------------------------------
# Part 1 Benchmarks
# ------------------------------------------------------------------------------
if [ "$MODE" = "all" ] || [ "$MODE" = "part1" ]; then
    echo "Running Part 1 Benchmarks..."
    echo "Clients,Throughput_MBps" > "$RESULTS_DIR/Part1_throughput.csv"
    for clients in 1 2 4 8; do
        cd "$ROOT_DIR/Part1"
        ./server 19080 "$DATASET_DIR" multi > server.log 2>&1 &
        SERVER_PID=$!
        sleep 1
        
        total_tp=0
        topic_count=0
        for topic in $TOPICS; do
            topic_count=$((topic_count + 1))
            for i in $(seq 1 $clients); do
                tp=$(./client 127.0.0.1 19080 "$topic" "out_${topic}_$i" 2>/dev/null | grep "Transfer Rate (Total)" | awk '{print $5}')
                if [ -z "$tp" ]; then tp="0.0"; fi
                total_tp=$(awk "BEGIN {print $total_tp + $tp}")
                rm -rf "out_${topic}_$i"
            done
        done
        
        avg_tp=$(awk "BEGIN {print $total_tp / ($clients * $topic_count)}")
        echo "$clients,$avg_tp" >> "$RESULTS_DIR/Part1_throughput.csv"
        kill -9 $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        cd "$SCRIPT_DIR"
    done
fi

# ------------------------------------------------------------------------------
# Part 2 Case 1 & Case 2 Benchmarks
# ------------------------------------------------------------------------------
if [ "$MODE" = "all" ] || [ "$MODE" = "part2" ]; then
    echo "Running Part 2 Case 1 & Case 2 Benchmarks..."
    echo "FailProb,Case1_Throughput_MBps,Case2_Throughput_MBps" > "$RESULTS_DIR/Part2_throughput.csv"
    for fail_prob in 0.00 0.01 0.02 0.05 0.10; do
        # Case 1
        cd "$ROOT_DIR/Part2_Case1"
        ./server 19081 "$DATASET_DIR" $fail_prob > server.log 2>&1 &
        SERVER_PID=$!
        sleep 1
        
        total_tp1=0
        topic_count=0
        for topic in $TOPICS; do
            topic_count=$((topic_count + 1))
            tp1=$(./client 127.0.0.1 19081 "$topic" "out_${topic}_c1" 2>/dev/null | grep "Throughput (Useful)" | awk '{print $4}')
            if [ -z "$tp1" ]; then tp1="0.0"; fi
            total_tp1=$(awk "BEGIN {print $total_tp1 + $tp1}")
            rm -rf "out_${topic}_c1"
        done
        kill -9 $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true

        # Case 2
        cd "$ROOT_DIR/Part2_Case2"
        ./server 19082 "$DATASET_DIR" $fail_prob > server.log 2>&1 &
        SERVER_PID=$!
        sleep 1
        
        total_tp2=0
        for topic in $TOPICS; do
            tp2=$(./client 127.0.0.1 19082 "$topic" "out_${topic}_c2" 2>/dev/null | grep "Throughput (Useful)" | awk '{print $4}')
            if [ -z "$tp2" ]; then tp2="0.0"; fi
            total_tp2=$(awk "BEGIN {print $total_tp2 + $tp2}")
            rm -rf "out_${topic}_c2"
        done
        kill -9 $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        
        cd "$SCRIPT_DIR"
        
        avg_tp1=$(awk "BEGIN {print $total_tp1 / $topic_count}")
        avg_tp2=$(awk "BEGIN {print $total_tp2 / $topic_count}")
        
        echo "$fail_prob,$avg_tp1,$avg_tp2" >> "$RESULTS_DIR/Part2_throughput.csv"
    done
fi

# ------------------------------------------------------------------------------
# Part 2 Case 2 Enhanced Benchmarks
# ------------------------------------------------------------------------------
if [ "$MODE" = "all" ] || [ "$MODE" = "part2_enhanced" ]; then
    echo "Running Part 2 Case 2 Enhanced Benchmarks..."
    echo "Connections,Throughput_MBps" > "$RESULTS_DIR/Part2_Enhanced_throughput.csv"
    for conn in 1 2 4 8; do
        cd "$ROOT_DIR/Part2_Case2_Enhanced"
        ./server 19083 "$DATASET_DIR" 0.05 > server.log 2>&1 &
        SERVER_PID=$!
        sleep 1
        
        total_tp=0
        topic_count=0
        for topic in $TOPICS; do
            topic_count=$((topic_count + 1))
            tp=$(./client 127.0.0.1 19083 "$topic" "out_${topic}_enh" $conn 2>/dev/null | grep "Throughput (Useful)" | awk '{print $4}')
            if [ -z "$tp" ]; then tp="0.0"; fi
            total_tp=$(awk "BEGIN {print $total_tp + $tp}")
            rm -rf "out_${topic}_enh"
        done
        kill -9 $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        cd "$SCRIPT_DIR"
        
        avg_tp=$(awk "BEGIN {print $total_tp / $topic_count}")
        echo "$conn,$avg_tp" >> "$RESULTS_DIR/Part2_Enhanced_throughput.csv"
    done
fi

echo "Done."
