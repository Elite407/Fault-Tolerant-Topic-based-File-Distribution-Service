#!/bin/bash

# Ensure output directories exist
mkdir -p ../Results

echo "Running Part 1 Benchmarks..."
echo "Clients,Throughput_MBps" > ../Results/Part1_throughput.csv
for clients in 1 2 4 8; do
    cd ../Part1
    ./server 8080 ../dataset > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # Run clients in parallel and extract average throughput
    total_tp=0
    for i in $(seq 1 $clients); do
        tp=$(./client 127.0.0.1 8080 Cane out_$i | grep "Transfer Rate (Total)" | awk '{print $5}')
        if [ -z "$tp" ]; then tp="0.0"; fi
        total_tp=$(awk "BEGIN {print $total_tp + $tp}")
    done
    
    avg_tp=$(awk "BEGIN {print $total_tp / $clients}")
    echo "$clients,$avg_tp" >> ../Results/Part1_throughput.csv
    kill $SERVER_PID
    cd ../benchmark
done

echo "Running Part 2 Case 1 & Case 2 Benchmarks..."
echo "FailProb,Case1_Throughput_MBps,Case2_Throughput_MBps" > ../Results/Part2_throughput.csv
for fail_prob in 0.00 0.01 0.02 0.05 0.10; do
    # Case 1
    cd ../Part2_Case1
    ./server 8081 ../dataset $fail_prob > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    tp1=$(./client 127.0.0.1 8081 Cane out_c1 | grep "Throughput (Useful)" | awk '{print $4}')
    kill $SERVER_PID

    # Case 2
    cd ../Part2_Case2
    ./server 8082 ../dataset $fail_prob > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    tp2=$(./client 127.0.0.1 8082 Cane out_c2 | grep "Throughput (Useful)" | awk '{print $4}')
    kill $SERVER_PID
    
    cd ../benchmark
    
    # Handle failures that timed out or dropped too much
    if [ -z "$tp1" ]; then tp1="0.0"; fi
    if [ -z "$tp2" ]; then tp2="0.0"; fi
    
    echo "$fail_prob,$tp1,$tp2" >> ../Results/Part2_throughput.csv
done

echo "Running Part 2 Case 2 Enhanced Benchmarks..."
echo "Connections,Throughput_MBps" > ../Results/Part2_Enhanced_throughput.csv
for conn in 1 2 4 8; do
    cd ../Part2_Case2_Enhanced
    ./server 8083 ../dataset 0.05 > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    tp=$(./client 127.0.0.1 8083 Cane out_enh $conn | grep "Throughput (Useful)" | awk '{print $4}')
    kill $SERVER_PID
    cd ../benchmark
    
    if [ -z "$tp" ]; then tp="0.0"; fi
    echo "$conn,$tp" >> ../Results/Part2_Enhanced_throughput.csv
done

echo "Done."
