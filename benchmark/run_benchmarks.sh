#!/bin/bash

# Ensure output directories exist
mkdir -p ../Results

TOPICS="cane cavallo elefante farfalla gallina gatto mucca pecora ragno scoiattolo"
NUM_TOPICS=10
DATASET_DIR="../../dataset/raw-img"

echo "Running Part 1 Benchmarks..."
echo "Clients,Throughput_MBps" > ../Results/Part1_throughput.csv
for clients in 1 2 4 8; do
    cd ../Part1
    ./server 8080 $DATASET_DIR > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # Run clients sequentially and extract average throughput
    total_tp=0
    for topic in $TOPICS; do
        for i in $(seq 1 $clients); do
            tp=$(./client 127.0.0.1 8080 $topic out_${topic}_$i | grep "Transfer Rate (Total)" | awk '{print $5}')
            if [ -z "$tp" ]; then tp="0.0"; fi
            total_tp=$(awk "BEGIN {print $total_tp + $tp}")
        done
    done
    
    avg_tp=$(awk "BEGIN {print $total_tp / ($clients * $NUM_TOPICS)}")
    echo "$clients,$avg_tp" >> ../Results/Part1_throughput.csv
    kill $SERVER_PID
    cd ../benchmark
done

echo "Running Part 2 Case 1 & Case 2 Benchmarks..."
echo "FailProb,Case1_Throughput_MBps,Case2_Throughput_MBps" > ../Results/Part2_throughput.csv
for fail_prob in 0.00 0.01 0.02 0.05 0.10; do
    # Case 1
    cd ../Part2_Case1
    ./server 8081 $DATASET_DIR $fail_prob > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    total_tp1=0
    for topic in $TOPICS; do
        tp1=$(./client 127.0.0.1 8081 $topic out_${topic}_c1 | grep "Throughput (Useful)" | awk '{print $4}')
        if [ -z "$tp1" ]; then tp1="0.0"; fi
        total_tp1=$(awk "BEGIN {print $total_tp1 + $tp1}")
    done
    kill $SERVER_PID

    # Case 2
    cd ../Part2_Case2
    ./server 8082 $DATASET_DIR $fail_prob > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    total_tp2=0
    for topic in $TOPICS; do
        tp2=$(./client 127.0.0.1 8082 $topic out_${topic}_c2 | grep "Throughput (Useful)" | awk '{print $4}')
        if [ -z "$tp2" ]; then tp2="0.0"; fi
        total_tp2=$(awk "BEGIN {print $total_tp2 + $tp2}")
    done
    kill $SERVER_PID
    
    cd ../benchmark
    
    avg_tp1=$(awk "BEGIN {print $total_tp1 / $NUM_TOPICS}")
    avg_tp2=$(awk "BEGIN {print $total_tp2 / $NUM_TOPICS}")
    
    echo "$fail_prob,$avg_tp1,$avg_tp2" >> ../Results/Part2_throughput.csv
done

echo "Running Part 2 Case 2 Enhanced Benchmarks..."
echo "Connections,Throughput_MBps" > ../Results/Part2_Enhanced_throughput.csv
for conn in 1 2 4 8; do
    cd ../Part2_Case2_Enhanced
    ./server 8083 $DATASET_DIR 0.05 > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    total_tp=0
    for topic in $TOPICS; do
        tp=$(./client 127.0.0.1 8083 $topic out_${topic}_enh $conn | grep "Throughput (Useful)" | awk '{print $4}')
        if [ -z "$tp" ]; then tp="0.0"; fi
        total_tp=$(awk "BEGIN {print $total_tp + $tp}")
    done
    kill $SERVER_PID
    cd ../benchmark
    
    avg_tp=$(awk "BEGIN {print $total_tp / $NUM_TOPICS}")
    echo "$conn,$avg_tp" >> ../Results/Part2_Enhanced_throughput.csv
done

echo "Done."
