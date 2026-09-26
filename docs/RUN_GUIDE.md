# End-to-End Execution Guide

This document provides a complete, step-by-step walkthrough to compile, run, and test every part of the **Fault-Tolerant Topic-Based File Distribution Service**.

To test the system effectively, it is highly recommended to open **two terminal windows** (or two split panes in your IDE/multiplexer):
*   **Terminal 1:** Dedicated to running the Server.
*   **Terminal 2:** Dedicated to running the Client.

Ensure you are in the project root directory in both terminals before beginning:
```bash
cd /home/darkknight007/Documents/CN_Project
```

---

## Phase 1: Test Part 1 (Basic Reliable Transfer)
This phase tests the foundational multi-threaded protocol over a reliable connection (no synthetic faults).

**1. Compile the code:**
```bash
cd Part1
make
```

**2. Start the Server (Terminal 1):**
*The server will listen on port `8080` and serve files located in the `../dataset` folder.*
```bash
./server 8080 ../dataset
```

**3. Start the Client (Terminal 2):**
*The client requests the topic `Cane` and saves downloaded files to an `output` folder.*
```bash
./client 127.0.0.1 8080 Cane output
```

**Observation:**
The client connects to the server and instantly downloads `file1.bin` and `file2.bin`. You can check the `output/Cane` folder to verify they arrived safely.
*(Stop the server by pressing `Ctrl+C` in Terminal 1 when finished).*

---

## Phase 2: Test Part 2 Case 1 (No Session Management)
This phase introduces simulated network failures. The server will randomly drop the connection, and because there is no session tracking, the client must restart the transfer from scratch every time it disconnects.

**1. Compile the code:**
```bash
cd ../Part2_Case1
make
```

**2. Start the Server (Terminal 1):**
*The `0.05` argument introduces a 5% probability that the server drops the TCP connection during any chunk transfer.*
```bash
./server 8081 ../dataset 0.05
```

**3. Start the Client (Terminal 2):**
```bash
./client 127.0.0.1 8081 Cane output
```

**Observation:**
You will see the connection abruptly drop, and the client will print `Connection closed... Reconnecting...`. Since it has no session state, it dumps its progress and starts over from byte 0. It will take multiple attempts to eventually succeed. Notice that the **Redundant Bytes** metric displayed at the end will be extremely high.
*(Stop the server by pressing `Ctrl+C` in Terminal 1).*

---

## Phase 3: Test Part 2 Case 2 (Session Management)
This phase implements strict session checkpointing. If the connection fails, the client remembers exactly how many bytes were safely written to disk and resumes from that specific point.

**1. Compile the code:**
```bash
cd ../Part2_Case2
make
```

**2. Start the Server (Terminal 1):**
```bash
./server 8082 ../dataset 0.05
```

**3. Start the Client (Terminal 2):**
```bash
./client 127.0.0.1 8082 Cane output
```

**Observation:**
The connection will still drop constantly due to the 5% failure probability. However, when the client reconnects, it sends its `Session ID` and exact byte offset. The server seeks to that exact byte and resumes. The transfer finishes much faster than in Case 1, and the **Redundant Bytes** metric will be practically zero, proving that no data was wastefully re-downloaded!
*(Stop the server by pressing `Ctrl+C` in Terminal 1).*

---

## Phase 4: Test Part 2 Enhanced (Parallel Connections)
This phase introduces client-side parallelization. The client spawns multiple concurrent worker threads to saturate the network and improve speeds.

**1. Compile the code:**
```bash
cd ../Part2_Case2_Enhanced
make
```

**2. Start the Server (Terminal 1):**
```bash
./server 8083 ../dataset 0.05
```

**3. Start the Client (Terminal 2):**
*The `4` at the end of the command tells the client to spawn 4 parallel worker threads.*
```bash
./client 127.0.0.1 8083 Cane output 4
```

**Observation:**
The client divides the workload across 4 parallel TCP connections. Even with a 5% failure rate tearing connections down, the topic downloads incredibly fast because failures on one thread don't block the others. If you look at the final metrics, the throughput numbers will be significantly higher than Case 2.
*(Stop the server by pressing `Ctrl+C` in Terminal 1).*

---

## Phase 5: Automated Benchmarks & Graph Generation
If you want to view the automated benchmarking suite in action (which runs the servers/clients in the background automatically and logs their metrics):

```bash
cd ../benchmark
./run_benchmarks.sh
```

**Observation:**
The script takes about 1-2 minutes to run. It automatically steps through different failure probabilities and parallel connection counts. Once it finishes, it generates raw CSV logs and plotted `.png` graphs in the `Results/` directory!
