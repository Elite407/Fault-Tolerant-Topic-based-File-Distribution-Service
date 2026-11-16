# Operational Run Guide & Multi-Node Physical Testing Manual
## Fault-Tolerant Topic-Based Binary File Distribution Service

**Course:** IT305: Computer Networks Laboratory  
**Institution:** Dhirubhai Ambani University (Formerly DA-IICT)  
**Project Group:** Group 100 & Group 113  
**Target Environment:** Linux POSIX (Ubuntu 20.04/22.04/24.04 LTS or WSL2) & Multi-Node Physical LAN  
**Companion Assets:** [`Images/ASSETS_INDEX.md`](Images/ASSETS_INDEX.md), [`README.md`](README.md), [`FinalReport_Group_100_113-1.pdf`](FinalReport_Group_100_113-1.pdf)

---

## 1. Executive Overview

This manual provides an exhaustive operational walkthrough for configuring, compiling, executing, and empirically verifying the **Fault-Tolerant Topic-Based Binary File Distribution Service**. It bridges the gap between synthetic single-host simulation and multi-node physical lab deployments, detailing:
1. Physical 3-node lab network topology and interface setup.
2. Complete test suite verification (`tests/run_tests.sh`) across all 15 integration tests.
3. Automated parameter sweep benchmarks (`benchmark/run_benchmarks.sh`) and 300 DPI analytical plotting (`benchmark/plot.py`).
4. Operational flow runners and logging harness (`scripts/execute_all_flows.sh`).
5. Dual-pane terminal proof card generation (`scripts/generate_terminal_cards.py`).
6. Empirical evidence collection protocols covering the 63 physical lab screenshots cataloged in [`Images/ASSETS_INDEX.md`](Images/ASSETS_INDEX.md).

---

## 2. Multi-Node Physical Lab Architecture & Topology

To validate protocol correctness and empirical throughput under real-world conditions, the service was evaluated across a dedicated 3-node physical Local Area Network (LAN) connected via a Gigabit Ethernet switch.

### 2.1 Physical Node Allocation Matrix

```
+---------------------------------------------------------------------------------------------------------+
|                                    PHYSICAL NETWORK TOPOLOGY                                            |
+---------------------------------------------------------------------------------------------------------+
|                                                                                                         |
|                      +---------------------------------------------------+                              |
|                      |             PHYSICAL GIGABIT SWITCH               |                              |
|                      |                 (192.168.1.0/24)                  |                              |
|                      +---------------------------------------------------+                              |
|                           |                           |                           |                     |
|                           |                           |                           |                     |
|           +---------------+                           |                           +---------------+     |
|           v                                           v                                           v     |
|   +-----------------------+                   +-----------------------+                   +-------------------+ |
|   |        NODE 1         |                   |        NODE 2         |                   |      NODE 3       | |
|   |    Server / Broker    |                   |     Client 1 (C1)     |                   | Client 2/3 (C2/C3)| |
|   |     192.168.1.100     |                   |     192.168.1.101     |                   |   192.168.1.102   | |
|   +-----------------------+                   +-----------------------+                   +-------------------+ |
|   | - Server Daemons      |                   | - Subscriber Node 1   |                   | - Subscriber 2    | |
|   | - Topics Catalog      |                   | - Case 1 Wipe Tests   |                   | - Subscriber 3    | |
|   | - Session Table       |                   | - Case 2 Resumption   |                   | - Parallel TCP    | |
|   | - Ports: 8080-8085    |                   | - Empirical Terminal  |                   | - Multi-client    | |
|   +-----------------------+                   +-----------------------+                   +-------------------+ |
|                                                                                                         |
+---------------------------------------------------------------------------------------------------------+
```

| Node Role | Identifier | Hostname / IP | OS Environment | Responsibilities |
|---|---|---|---|---|
| **Server / Broker** | Node 1 | `broker-srv` (`192.168.1.100`) | Ubuntu 22.04 LTS | Hosts binary dataset (`raw-img`), manages topic directory indexing, tracks active session checkpoints, and injects probabilistic socket drops. |
| **Subscriber 1** | Node 2 (C1) | `client-node-01` (`192.168.1.101`) | Ubuntu 22.04 LTS | Primary empirical subscriber. Executes single-thread/multi-thread pulls, measures recovery overhead under fault injection, and captures terminal evidence. |
| **Subscriber 2 & 3** | Node 3 (C2/C3) | `client-node-02` (`192.168.1.102`) | Ubuntu 22.04 LTS | Concurrent client subscriber node. Launches simultaneous transfers to evaluate server concurrency, lock contention, and parallel TCP streaming. |

### 2.2 Network Interface & Firewall Configuration

Before starting physical node tests, ensure ports `8080` through `8085` and `19101` through `19105` are accessible through host firewalls:

```bash
# On Node 1 (Server):
sudo ufw allow 8080:8085/tcp comment "TFDP Server Ports"
sudo ufw allow 19101:19105/tcp comment "TFDP Flow Harness Ports"
sudo ufw reload

# Verify open ports and network reachability from Node 2 & Node 3:
ping -c 3 192.168.1.100
nc -zv 192.168.1.100 8080
```

---

## 3. End-to-End Automated Test Suite Verification

The repository includes a turnkey bash test harness in `tests/run_tests.sh` that validates compilation, boundary framing, error handling, session resumption, and multi-worker partitioning across 15 automated checks.

### 3.1 Executing the Test Suite

Under Linux or WSL2:
```bash
cd tests
chmod +x run_tests.sh
./run_tests.sh
```

### 3.2 Automated Test Breakdown

The 15 integration tests execute in sequential stages:
1. **Part 1 Compilation:** Invokes `make clean && make` in `Part1/` with `-Wall -Wextra -pthread -lrt`.
2. **Part 1 Transfer Integrity:** Transfers a multi-file synthetic dataset (text, 128 KB binary, 1 MB multi-chunk binary) over loopback and validates byte-for-byte exactness via `diff -r`.
3. **Part 1 Negative Error Handling:** Requests non-existent topic `non_existent_topic_xyz` and confirms clean `TYPE_ERROR` rejection without socket hangs.
4. **Part 2 Case 1 Compilation:** Builds the unmanaged fault-injection server and wipe-on-drop client.
5. **Part 2 Case 1 Baseline:** Executes clean transfer at $P=0.0$ and checks hash match.
6. **Part 2 Case 1 Fault Recovery:** Injects $P=0.02$ fault rate, verifies that client triggers `clean_topic_dir()`, reconnects, and completes with verified byte-for-byte checksums.
7. **Part 2 Case 2 Compilation:** Builds stateful session server and checkpointing client.
8. **Part 2 Case 2 Baseline:** Validates zero-fault baseline transfer.
9. **Part 2 Case 2 Stateful Resumption:** Injects $P=0.02$ fault rate, drops connection, sends `TYPE_RECONNECT_REQ` carrying `session_id` and byte offset, validates server `fseek()` resumption, and verifies 100% checksum match with 0 redundant retransmissions.
10. **Part 2 Case 2 Enhanced Compilation:** Builds multi-worker parallel streaming client and server.
11. **Part 2 Case 2 Enhanced Concurrency:** Spawns 2 concurrent worker connections, partitions workload via `file_idx % total_workers`, and validates complete reconstructed output.
12. **FinalSubmission Part 1 Build:** Confirms `FinalSubmission_Group_100_113/Part1` builds cleanly.
13. **FinalSubmission Part 2 Case 1 Build:** Confirms `FinalSubmission_Group_100_113/Part2_Case1` builds cleanly.
14. **FinalSubmission Part 2 Case 2 Build:** Confirms `FinalSubmission_Group_100_113/Part2_Case2` builds cleanly.
15. **FinalSubmission Part 2 Case 2 Enhanced Build:** Confirms `FinalSubmission_Group_100_113/Part2_Case2_Enhanced` builds cleanly.

Expected terminal output:
```text
================================================================
                       Test Suite Summary                       
================================================================
Total Tests  : 15
Passed Tests : 15
Failed Tests : 0

🎉 ALL TESTS PASSED!
```

---

## 4. Automated Benchmarking & Publication Graph Generation

The benchmark suite sweeps system parameters against real-world image datasets (e.g., Animals-10 `raw-img`), logging raw throughput, reconnect events, useful bytes, and redundant overhead.

### 4.1 Running the Benchmark Sweep

```bash
cd benchmark
chmod +x run_benchmarks.sh
./run_benchmarks.sh
```

The script executes three sequential parameter sweeps:
1. **Part 1 Multi-Client Scalability:** Sweeps concurrency across 1, 2, 4, and 8 concurrent client processes requesting topics simultaneously.
2. **Part 2 Case 1 vs. Case 2 Resilience:** Sweeps synthetic failure probabilities $P \in \{0.00, 0.01, 0.02, 0.05, 0.10\}$ on identical datasets.
3. **Part 2 Case 2 Enhanced Concurrency:** Sweeps parallel worker counts (1, 2, 4, 8 TCP streams) under constant 5% loss ($P=0.05$).

### 4.2 Verified Benchmark Datasets

Benchmark outputs are written to `Results/`:
- `Results/Part1_throughput.csv`
- `Results/Part2_throughput.csv`
- `Results/Part2_Enhanced_throughput.csv`

```text
# Results/Part1_throughput.csv
Clients,Throughput_MBps
1,52.61
2,102.645
4,102.255
8,112.156

# Results/Part2_throughput.csv
FailProb,Case1_Throughput_MBps,Case2_Throughput_MBps
0.00,63.50,301.60
0.01,0.27,1.05
0.02,0.05,1.34

# Results/Part2_Enhanced_throughput.csv
Connections,Throughput_MBps
1,0.42
2,0.66
4,2.04
8,2.46
```

### 4.3 Generating 300 DPI Publication Plots

To render high-resolution plots from the CSV metrics:
```bash
python benchmark/plot.py
```
This produces three publication-grade figures in `Results/`:
- `Results/Part1_throughput.png` (2656x1619 px, 300 DPI)
- `Results/Part2_Cases_throughput.png` (2820x1619 px, 300 DPI)
- `Results/Part2_Enhanced_throughput.png` (2655x1619 px, 300 DPI)

---

## 5. Operational Flow Runners & Terminal Proof Card Reproduction

To capture end-to-end operational proof-of-work across all architectural tiers, the repository provides automated flow runners and image generation utilities in `scripts/`.

### 5.1 Executing All System Flows

```bash
chmod +x scripts/execute_all_flows.sh
./scripts/execute_all_flows.sh
```

This harness executes and logs:
- **Broker Startup:** Port binding, topic catalog indexing, socket listen state (`logs/broker_startup.log`).
- **Part 1 Transfer:** Clean high-throughput multi-file transfer (`logs/part1_server.log`, `logs/part1_client.log`).
- **Part 2 Case 1 Failover:** Connection drops, recursive directory wipes (`logs/part2_c1_server.log`, `logs/part2_c1_client.log`).
- **Part 2 Case 2 Resumption:** Session table registration, byte-offset checkpoints, zero redundant bytes (`logs/part2_c2_server.log`, `logs/part2_c2_client.log`).
- **Part 2 Case 2 Enhanced:** 4-worker parallel TCP streaming and aggregate throughput (`logs/part2_enh_server.log`, `logs/part2_enh_client.log`).

### 5.2 Rendering Dual-Pane Terminal Proof Cards

To re-render the 6 executive dual-pane terminal proof cards in `Images/`:
```bash
python scripts/generate_terminal_cards.py
```
Outputs generated in `Images/`:
- `Images/terminal_broker_startup.png` (Server daemon initialization & 10 topics indexed)
- `Images/terminal_part1_transfer.png` (12 files transferred at 52.6 MB/s)
- `Images/terminal_part2_case1_failover.png` (Drop detection, directory wipe, 44.7% redundant overhead)
- `Images/terminal_part2_case2_session_resume.png` (28 drops, byte-offset resumption, 0 redundant bytes)
- `Images/terminal_part2_enhanced_parallel.png` (4 parallel TCP workers, 5.2 MB/s aggregate throughput)
- `Images/terminal_tests_verification.png` (Full 15/15 test suite PASS confirmation)

---

## 6. Physical Multi-Node Evidence Collection Runbook

This section details how the 63 empirical client screenshots cataloged in [`Images/ASSETS_INDEX.md`](Images/ASSETS_INDEX.md) were collected across physical nodes C1, C2, and C3.

### 6.1 Part 1 Physical Lab Run (Single vs Multi-Threaded)

1. **Start Server on Node 1 (`192.168.1.100`):**
   ```bash
   cd Part1
   ./server 8080 "/mnt/d/dataset/raw-img" multi
   ```
2. **Execute Single-Client Test on Node 2 (C1):**
   ```bash
   cd Part1
   ./client 192.168.1.100 8080 cane ./output_c1
   # Evidence capture: Images/C1_Part1_Multi_thread_1c.png
   ```
3. **Execute Concurrent 2-Client Test on Nodes 2 & 3 (C1 & C2):**
   ```bash
   # On Node 2 (C1):
   ./client 192.168.1.100 8080 cane ./output_c1
   # On Node 3 (C2):
   ./client 192.168.1.100 8080 cavallo ./output_c2
   # Evidence captures: Images/C1_Part1_Multi_thread_2c.png, Images/C2_Part1_Single_Thread_2c.png
   ```
4. **Execute 3-Client Test (C1, C2, C3):**
   ```bash
   # Launch concurrent clients on C1, C2, and C3.
   # Evidence captures: Images/c1_Part1_MultiThread_3c.png, Images/c2_Part1_MultiThread_2c.png, Images/c3_Part1_MultiThread_3c.png
   ```

### 6.2 Part 2 Case 1 Physical Lab Run (Wipe-on-Disconnect)

1. **Start Fault-Injecting Server on Node 1:**
   ```bash
   cd Part2_Case1
   ./server 8081 "/mnt/d/dataset/raw-img" 0.05
   ```
2. **Execute Client on Node 2 (C1) under $P=0.05$:**
   ```bash
   cd Part2_Case1
   ./client 192.168.1.100 8081 cane ./output_case1
   # Notice high Redundant Bytes and multiple reconnect loops.
   # Evidence capture: Images/C1_Part2_Case1_p=0.05_1c.png
   ```
3. **Execute under $P=0.10$ and $P=0.50$:**
   ```bash
   # Evidence captures: Images/C1_Part2_case1_p=0.1_1c.png, Images/C2_Part2_case1_p=0.5_2c.png
   ```

### 6.3 Part 2 Case 2 Physical Lab Run (Stateful Resumption)

1. **Start Stateful Server on Node 1:**
   ```bash
   cd Part2_Case2
   ./server 8082 "/mnt/d/dataset/raw-img" 0.05
   ```
2. **Execute Resilient Client on Node 2 (C1):**
   ```bash
   cd Part2_Case2
   ./client 192.168.1.100 8082 cane ./output_case2
   # Terminal displays: "Resumed from File idx X, Offset Y"
   # Redundant Bytes = 0 bytes!
   # Evidence captures: Images/c1_Part2_Case2_p=0.05_1c(1).png, Images/c1_Part2_Case2_p=0.05_1c(2).png
   ```
3. **Execute Concurrent 2-Client Resumption on Nodes 2 & 3:**
   ```bash
   # Evidence captures: Images/c1_Part2_Case2_p=0.05_2c(1).png, Images/C2_Part2_case2_p=0.05_2c(1).png
   ```

### 6.4 Part 2 Case 2 Enhanced Physical Lab Run (Parallel TCP Streaming)

1. **Start Modulo Partitioning Server on Node 1:**
   ```bash
   cd Part2_Case2_Enhanced
   ./server 8083 "/mnt/d/dataset/raw-img" 0.05
   ```
2. **Execute 4-Worker Parallel Client on Node 2 (C1):**
   ```bash
   cd Part2_Case2_Enhanced
   ./client 192.168.1.100 8083 cane ./output_enhanced 4
   # 4 parallel worker threads stream distinct subsets of files concurrently.
   # Evidence captures: Images/c1_Part2_Case2_En_p=0.05_1c(1).png, Images/c1_Part2_Case2_En_p=0.05_1c(2).png
   ```

---

## 7. Operational Troubleshooting & Runbook

### 7.1 Port In-Use Errors (`EADDRINUSE`)
If a server terminates abruptly, the listening socket may linger in `TIME_WAIT`:
```bash
# Locate lingering process holding port 8080:
sudo lsof -i :8080
# Or using ss:
ss -tulpn | grep 8080

# Kill lingering server process:
sudo fuser -k 8080/tcp
```
*Note: All server implementations include `setsockopt(..., SO_REUSEADDR, ...)` to minimize `TIME_WAIT` binding collisions.*

### 7.2 WSL2 Subsystem Reset
If WSL2 background sockets become unresponsive during heavy parallel benchmark sweeps:
```powershell
# From Windows PowerShell:
wsl --terminate Ubuntu
wsl -d Ubuntu
```

### 7.3 Output Directory Verification & Cleaning
To manually verify download integrity between test runs:
```bash
# Verify downloaded files against source topic:
diff -r output_case2/cane "/mnt/d/dataset/raw-img/cane"

# Clean test output folders:
rm -rf output_p1 output_case1 output_case2 output_enhanced
```

---

*This guide forms part of the project verification and portfolio showcase suite. For high-level architectural details and embedded visual results, refer to [`README.md`](README.md).*
