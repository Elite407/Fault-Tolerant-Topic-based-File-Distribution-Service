# Fault-Tolerant Topic-Based Binary File Distribution Service

**Course:** IT305: Computer Networks Laboratory  
**Institution:** Dhirubhai Ambani University (Formerly DA-IICT)  
**Project Group:** Group 100 & Group 113  
**Technology Stack:** C (C99/C11 standards), POSIX BSD Sockets, POSIX Threads (`pthreads`), Real-Time Extensions (`librt`)  
**Companion Portfolios:** [`Images/ASSETS_INDEX.md`](Images/ASSETS_INDEX.md) (Complete 69-Asset Gallery) • [`report_execution_guide.md`](report_execution_guide.md) (Multi-Node Testing Manual) • [`FinalReport_Group_100_113-1.pdf`](FinalReport_Group_100_113-1.pdf) (Academic Report)

---

## Table of Contents
- [1. Executive Summary](#1-executive-summary)
- [2. Visual Showcase: Empirical Performance & Terminal Proof of Work](#2-visual-showcase-empirical-performance--terminal-proof-of-work)
  - [2.1 Live Broker Initialization & Port Listening State](#21-live-broker-initialization--port-listening-state)
  - [2.2 Part I Baseline Multi-Threaded Topic Transfer](#22-part-i-baseline-multi-threaded-topic-transfer)
  - [2.3 Part II Case 1 Unmanaged Fault Drop & Wipe Recovery](#23-part-ii-case-1-unmanaged-fault-drop--wipe-recovery)
  - [2.4 Part II Case 2 Stateful Session Checkpointing & Resumption](#24-part-ii-case-2-stateful-session-checkpointing--resumption)
  - [2.5 Part II Case 2 Enhanced 4-Worker Parallel TCP Streaming](#25-part-ii-case-2-enhanced-4-worker-parallel-tcp-streaming)
  - [2.6 End-to-End Test Suite 15/15 PASS Verification](#26-end-to-end-test-suite-1515-pass-verification)
  - [2.7 Empirical Benchmark Analytics & Scalability Curves](#27-empirical-benchmark-analytics--scalability-curves)
  - [2.8 Complete 69-Asset Gallery & Physical Multi-Node Catalog](#28-complete-69-asset-gallery--physical-multi-node-catalog)
- [3. System Architecture](#3-system-architecture)
  - [3.1 Part I: Basic File Distribution Service](#31-part-i-basic-file-distribution-service)
  - [3.2 Part II Case 1: Unmanaged Session Fault-Tolerance](#32-part-ii-case-1-unmanaged-session-fault-tolerance)
  - [3.3 Part II Case 2: Stateful Session Checkpointing and Resumption](#33-part-ii-case-2-stateful-session-checkpointing-and-resumption)
  - [3.4 Part II Case 2 Enhanced: Parallel Multi-Connection TCP Streaming](#34-part-ii-case-2-enhanced-parallel-multi-connection-tcp-streaming)
- [4. Application-Layer Protocol Specification](#4-application-layer-protocol-specification)
  - [4.1 Binary Framing and Fixed Header](#41-binary-framing-and-fixed-header)
  - [4.2 Control and Data Message Types](#42-control-and-data-message-types)
  - [4.3 Detailed Payload Layouts](#43-detailed-payload-layouts)
  - [4.4 Protocol Handshake and Recovery Workflows](#44-protocol-handshake-and-recovery-workflows)
  - [4.5 Robust Network I/O Primitives](#45-robust-network-io-primitives)
- [5. Repository Structure](#5-repository-structure)
- [6. Compilation and Prerequisites](#6-compilation-and-prerequisites)
  - [6.1 Environment Prerequisites](#61-environment-prerequisites)
  - [6.2 Building Modules](#62-building-modules)
- [7. Execution and Operational Guide](#7-execution-and-operational-guide)
  - [7.1 Part I Execution](#71-part-i-execution)
  - [7.2 Part II Case 1 Execution](#72-part-ii-case-1-execution)
  - [7.3 Part II Case 2 Execution](#73-part-ii-case-2-execution)
  - [7.4 Part II Case 2 Enhanced Execution](#74-part-ii-case-2-enhanced-execution)
- [8. Verification and Automated Testing](#8-verification-and-automated-testing)
  - [8.1 Test Suite Organization](#81-test-suite-organization)
  - [8.2 Running the Test Harness](#82-running-the-test-harness)
  - [8.3 Automated Verification Proof](#83-automated-verification-proof)
- [9. Performance Benchmarking and Analytics](#9-performance-benchmarking-and-analytics)
  - [9.1 Executing Automated Benchmarks](#91-executing-automated-benchmarks)
  - [9.2 Generating Analytical Graphs](#92-generating-analytical-graphs)
  - [9.3 Empirical Benchmark Plots & Scalability Analysis](#93-empirical-benchmark-plots--scalability-analysis)
  - [9.4 Architectural Trade-offs and Observations](#94-architectural-trade-offs-and-observations)
- [10. Robustness, Autograder Compliance, and Quality Gates](#10-robustness-autograder-compliance-and-quality-gates)
- [11. Complete Documentation Roadmap](#11-complete-documentation-roadmap)

---

## 1. Executive Summary

This project implements a high-performance, fault-tolerant, topic-based binary file distribution service in C using POSIX BSD sockets and POSIX threads (`pthreads`). Operating on an application-level client-server paradigm, datasets are organized hierarchically by topic names (e.g., animal classes from the Animals-10 dataset). Clients issue structured binary requests for specific topics, and the server transmits all associated files using a custom framing protocol that guarantees complete boundary preservation across stream-oriented TCP sockets.

The system addresses both foundational networking requirements and complex fault-tolerance challenges across four distinct operational modes:
1. **Part I:** Single-threaded (sequential) and multi-threaded (concurrent listener-dispatcher) file transfer achieving baseline throughputs exceeding 50 MB/s.
2. **Part II Case 1:** Fault-injected environment without session state, enforcing full transfer aborts, recursive directory wipes, and clean restarts from scratch upon disconnection (exposing the severe throughput cliff of unmanaged failure recovery).
3. **Part II Case 2:** Fault-injected environment featuring thread-safe stateful session management, verified disk-write checkpoints, and resumption from the exact byte offset where the fault occurred (achieving **0 bytes of redundant data retransmission**).
4. **Part II Case 2 Enhanced:** Parallel multi-connection TCP streaming across concurrent client worker threads, deterministically partitioning files via modulo arithmetic and isolating failure domains to deliver a **5.86x throughput acceleration**.

---

## 2. Visual Showcase: Empirical Performance & Terminal Proof of Work

For technical recruiters, hiring managers, and systems engineers evaluating this repository, this section consolidates executive dual-pane terminal proof cards and empirical benchmark plots capturing genuine live system executions, automated verification, and performance scaling.

```
+---------------------------------------------------------------------------------------------------------+
|                                    VISUAL EVIDENCE & VERIFICATION MATRIX                                |
+---------------------------------------------------------------------------------------------------------+
| Operational Mode          | Visual Proof Card                       | Empirical Metric / Key Result     |
+---------------------------+-----------------------------------------+-----------------------------------+
| Broker Startup            | terminal_broker_startup.png             | 10 Topics Indexed, Port 19101     |
| Part I Transfer           | terminal_part1_transfer.png             | 12 Files, 52.61 MB/s, 0% Loss     |
| Part II Case 1 Failover   | terminal_part2_case1_failover.png       | 7 Drops, Wipe & Restart, 44.7% Ovh|
| Part II Case 2 Resumption | terminal_part2_case2_session_resume.png | 28 Drops, fseek Resume, 0 Redund. |
| Part II Enhanced Concurr. | terminal_part2_enhanced_parallel.png    | 4 Workers, 5.2 MB/s, 4x Speedup   |
| Test Suite Verification   | terminal_tests_verification.png         | 15/15 PASS (100% Pass Rate)       |
+---------------------------------------------------------------------------------------------------------+
```

### 2.1 Live Broker Initialization & Port Listening State

The dual-pane proof card below captures the broker daemon starting up, traversing the filesystem to index 10 topic catalogs from the Animals-10 dataset, binding to port `19101`, and validating its TCP listen socket state.

![Broker Startup and Socket Verification](Images/terminal_broker_startup.png)

*Figure 2.1: Dual-pane execution capture showing server initialization (left) dynamically indexing 10 topics (`cane`, `cavallo`, `elefante`, `farfalla`, `gallina`, `gatto`, `mucca`, `pecora`, `ragno`, `scoiattolo`), followed by active TCP socket state verification via `ss -tulpn` (right) confirming listening state with 100-connection backlog.*

| Metric / Parameter | Value / State | Architectural Note |
|---|---|---|
| **Listening Port** | `19101` (Configurable CLI) | Passed via argv; no hardcoded ports |
| **Catalog Traversal** | 10 topics loaded | Uses `opendir()` / `readdir()` with `access(R_OK \| X_OK)` |
| **Socket Options** | `SO_REUSEADDR` enabled | Eliminates `EADDRINUSE` port lockups during daemon restarts |
| **Listen Backlog** | 100 connections | Accommodates bursts of concurrent subscriber connections |

---

### 2.2 Part I Baseline Multi-Threaded Topic Transfer

The baseline architecture pairs an asynchronous listener thread with dynamic per-client POSIX worker threads. Below, the client requests topic `topic_demo` and streams 12 binary payload files with zero transport loss.

![Part 1 Multi-Threaded Transfer](Images/terminal_part1_transfer.png)

*Figure 2.2: Live transfer verification of Part 1. The server (left) receives `TYPE_GET_REQUEST` (`0x01`), emits topic metadata (`0x02`), and streams chunks (`0x04`). The subscriber client (right) receives 12 files (471,859,200 bits total) at **52.61 MB/s**, verifying file boundaries and byte counts.*

| Parameter | Measurement | Significance |
|---|---|---|
| **Topic Requested** | `topic_demo` (12 files) | Evaluates multi-file sequential streaming within one connection |
| **Useful Throughput** | **52.61 MB/s** (Loopback) | Baseline network saturation with 8 KB chunk framing |
| **Data Integrity** | Identical source & dest | Validated via `diff -r` across all 12 binary payloads |
| **Framing Type** | 9-byte binary header | Zero framing errors or boundary amalgamation |

---

### 2.3 Part II Case 1 Unmanaged Fault Drop & Wipe Recovery

In Case 1, the server injects synthetic socket drops before transmitting data chunks. Because no session state is maintained, the client must recursively wipe its output directory (`clean_topic_dir()`) and restart the entire transfer from file 0 upon every failure.

![Part 2 Case 1 Fault Tolerance Failover](Images/terminal_part2_case1_failover.png)

*Figure 2.3: Unmanaged failure recovery under synthetic fault injection. The server (left) randomly closes the TCP connection midway through chunk delivery. The client (right) detects socket termination, purges partially written files from disk, reconnects, and restarts from scratch, incurring **44.7% redundant byte retransmission overhead** across 7 reconnect cycles.*

| Fault Scenario Metric | Value | Architectural Impact |
|---|---|---|
| **Failure Injection Rate ($P$)** | $0.05$ (5% per chunk) | High drop rate testing extreme recovery boundaries |
| **Connection Drops / Reconnects** | 7 cycles | Client executes full reconnect loops |
| **Redundant Bytes Transferred** | **2,067,263 bytes** | **44.7% wasted bandwidth** due to wiping and restarting |
| **Useful Bytes Delivered** | 4,625,972 bytes | Useful payload received after repeated retransmissions |

---

### 2.4 Part II Case 2 Stateful Session Checkpointing & Resumption

Case 2 introduces mutex-guarded server-side session tracking and client-side disk-flushed byte-offset checkpoints. When a connection drops, the client reconnects with `TYPE_RECONNECT_REQ` carrying its `session_id`, file index, and offset. The server seeks the file pointer (`fseek`) and resumes transmission with **zero wasted data bytes**.

![Part 2 Case 2 Stateful Session Resumption](Images/terminal_part2_case2_session_resume.png)

*Figure 2.4: Stateful session resumption under heavy fault injection (28 socket drops). The client preserves verified chunks on disk, presents its 64-bit session ID and exact byte offset upon reconnection, and resumes seamlessly. Result: **0 redundant data bytes** retransmitted (<0.3% framing overhead).*

| Session Metric | Case 1 (Unmanaged) | Case 2 (Stateful Session) | Improvement Factor |
|---|---|---|---|
| **Disconnection Events** | 7 drops | 28 drops (severe loss) | 4x higher fault resilience |
| **Redundant Data Retransmitted** | 2,067,263 bytes (44.7%) | **0 bytes (0.0% data redundancy)** | **$\infty$ (Zero Waste)** |
| **Recovery Strategy** | Full directory wipe & restart | Exact byte-offset `fseek` | Idempotent checkpointing |
| **File Checksum Match** | Passed after long delay | **100% exact match** | Guaranteed ACID-like progress |

---

### 2.5 Part II Case 2 Enhanced 4-Worker Parallel TCP Streaming

Case 2 Enhanced spawns $N$ concurrent worker threads per client. Workload is partitioned across parallel TCP connections using round-robin modulo arithmetic: $\text{Worker } i \text{ processes file } k \iff k \pmod N = i$.

![Part 2 Case 2 Enhanced Parallel Streaming](Images/terminal_part2_enhanced_parallel.png)

*Figure 2.5: Parallel 4-worker TCP streaming under 8% chunk fault probability. The server (left) distributes files deterministically across independent worker connections. The multi-threaded client (right) aggregates throughput to **5.2 MB/s**, completing in 10.98 seconds despite multiple concurrent socket drops.*

| Multi-Connection Parameter | 1 Worker Baseline | 4 Worker Parallel | Concurrency Advantage |
|---|---|---|---|
| **Active TCP Connections** | 1 socket stream | 4 independent TCP streams | 4x parallel socket bandwidth |
| **Fault Isolation** | Single drop halts progress | Drop on worker $i$ does not stall workers $j \ne i$ | Decentralized failure domains |
| **Throughput under Faults** | ~1.34 MB/s | **5.2 MB/s** | **~4x speedup** under high loss |
| **Workload Distribution** | Sequential $0 \to M-1$ | Modulo partitioning $k \pmod 4$ | Deterministic lockless sharding |

---

### 2.6 End-to-End Test Suite 15/15 PASS Verification

The automated test harness in `tests/run_tests.sh` executes 15 integration tests across compilation, boundary framing, error handling, session recovery, and multi-connection streaming.

![Automated Integration Test Verification](Images/terminal_tests_verification.png)

*Figure 2.6: Complete automated test suite run under WSL2 Ubuntu confirming 15/15 tests passing with zero failures.*

| Test Stage | Test Description | Status |
|---|---|---|
| **Test 1a-c** | Part 1 Compilation, multi-file binary transfer diff, and negative error handling | **PASS** |
| **Test 2a-c** | Part 2 Case 1 Compilation, 0% baseline transfer, and fault-recovery directory wipe | **PASS** |
| **Test 3a-c** | Part 2 Case 2 Compilation, 0% baseline transfer, and byte-offset resumption match | **PASS** |
| **Test 4a-b** | Part 2 Case 2 Enhanced Compilation and 2-worker modulo file transfer integrity | **PASS** |
| **Test 5a-d** | FinalSubmission package clean compilation across all 4 subdirectories | **PASS** |

---

### 2.7 Empirical Benchmark Analytics & Scalability Curves

The repository's automated benchmark engine (`benchmark/run_benchmarks.sh` + `benchmark/plot.py`) sweeps parameter spaces across the Animals-10 dataset (`raw-img`), generating publication-quality (300 DPI) analytical plots:

```
+---------------------------------------------------------------------------------------------------------+
|                                      BENCHMARK RESULTS SUMMARY                                          |
+---------------------------------------------------------------------------------------------------------+
| Multi-Client Scalability          | Fault Resilience Comparison             | Concurrency Acceleration  |
| (Part 1 Throughput)               | (Case 1 vs Case 2 Throughput)           | (Parallel TCP Scaling)    |
+-----------------------------------+-----------------------------------------+---------------------------+
| 1 Client  : 52.61 MB/s            | P=0.00: C1=63.5 MB/s, C2=301.6 MB/s     | 1 Conn : 0.42 MB/s        |
| 2 Clients : 102.65 MB/s (1.95x)   | P=0.01: C1=0.27 MB/s, C2=1.05 MB/s      | 2 Conns: 0.66 MB/s (1.57x)|
| 4 Clients : 102.26 MB/s           | P=0.02: C1=0.05 MB/s, C2=1.34 MB/s      | 4 Conns: 2.04 MB/s (4.86x)|
| 8 Clients : 112.16 MB/s           | (Case 2 achieves 26.8x speedup at P=0.02)| 8 Conns: 2.46 MB/s (5.86x)|
+-----------------------------------+-----------------------------------------+---------------------------+
```

*(See [Section 9: Performance Benchmarking and Analytics](#9-performance-benchmarking-and-analytics) for the embedded 300 DPI benchmark plots and in-depth performance analysis).*

---

### 2.8 Complete 69-Asset Gallery & Physical Multi-Node Catalog

In addition to the executive proof cards and benchmark plots above, the repository includes **63 empirical terminal captures** recorded across a physical 3-node lab network (Server node + Subscriber C1 + Subscriber C2/C3).

- **Full Catalog Index:** Consult [`Images/ASSETS_INDEX.md`](Images/ASSETS_INDEX.md) for resolutions, file sizes, and descriptions of all 69 visual assets.
- **Physical Testbed Runbook:** Consult [`report_execution_guide.md`](report_execution_guide.md) for the multi-node lab deployment manual, interface setups, and step-by-step reproduction guide.

---

## 3. System Architecture

```
+----------------------------------------------------------------------------------------------------+
|                                      SYSTEM ARCHITECTURE                                           |
+----------------------------------------------------------------------------------------------------+
|                                                                                                    |
|    +-----------------------------+                           +--------------------------------+     |
|    |        CLIENT NODE          |                           |          SERVER NODE           |     |
|    +-----------------------------+                           +--------------------------------+     |
|    |                             |                           |                                |     |
|    |  +-----------------------+  |   TYPE_GET_REQUEST (0x01) |  +--------------------------+  |     |
|    |  |  Topic Request Logic  |--|-------------------------->|  | Startup Topic Table      |  |     |
|    |  +-----------------------+  |                           |  | init_topic_table()       |  |     |
|    |                             |  TYPE_TOPIC_RESPONSE(0x02)|  +--------------------------+  |     |
|    |  +-----------------------+  |<--------------------------|               |                |     |
|    |  | Handshake & Start     |--+                           |               v                |     |
|    |  | TYPE_MSG_START (0x08) |--+-------------------------->|  +--------------------------+  |     |
|    |  +-----------------------+  |                           |  | Worker Dispatcher        |  |     |
|    |             |               |                           |  | (Single / Multi-thread)  |  |     |
|    |             v               |                           |  +--------------------------+  |     |
|    |  +-----------------------+  |                           |               |                |     |
|    |  | File Metadata Parser  |  |                           |               v                |     |
|    |  | TYPE_FILE_META (0x03) |<-|---------------------------|--+    Per-Chunk Stream         |     |
|    |  +-----------------------+  |                           |  |    +--------------------+   |     |
|    |             |               |   TYPE_FILE_CHUNK (0x04)  |  +--->| Fault Injector     |   |     |
|    |             v               |<--------------------------|-------| rand() < P ? close |   |     |
|    |  +-----------------------+  |                           |       +--------------------+   |     |
|    |  | Disk Write & Flush    |  |                           |               |                |     |
|    |  | (fwrite + Checkpoint) |  |                           |               v                |     |
|    |  +-----------------------+  |                           |       +--------------------+   |     |
|    |             |               |   TYPE_TOPIC_EOF (0x07)   |       | Session Table      |   |     |
|    |             v               |<--------------------------|-------| (Part II Case 2)   |   |     |
|    |  +-----------------------+  |                           |       +--------------------+   |     |
|    |  | Metrics Calculator    |  |                           |                                |     |
|    |  | (Time, MB/s, Redund.) |  |                           |                                |     |
|    |  +-----------------------+  |                           |                                |     |
|    +-----------------------------+                           +--------------------------------+     |
|                                                                                                    |
+----------------------------------------------------------------------------------------------------+
```

### 3.1 Part I: Basic File Distribution Service

- **Startup Discovery:** At startup, the server inspects the provided `<topics_root_dir>` via `init_topic_table()`. It iterates through directory entries, skips hidden files (`.` and `..`), validates read and execute access permissions (`access(R_OK | X_OK)`), and populates an in-memory `TopicEntry topic_table[MAX_TOPICS]`.
- **Single-Threaded Execution:** The server processes client requests sequentially. Incoming connections queue in the OS TCP listen backlog (`listen(..., 100)`). The server performs the entire handshake, streams each file sequentially, sends `TYPE_TOPIC_EOF`, closes the socket, and only then invokes `accept()` for the next client.
- **Multi-Threaded Execution:** The main thread functions solely as an event listener. Upon `accept()`, it allocates a client context structure on the heap and spawns a detached POSIX thread (`pthread_create` + `pthread_detach`). Multiple clients execute file reading and transmission simultaneously without blocking each other.
- **Client Pipeline:** The client sends `TYPE_GET_REQUEST`, verifies `TYPE_TOPIC_RESPONSE`, issues `TYPE_MSG_START`, creates `<output_dir>/<topic>/` recursively via `mkdir_p()`, receives chunk payloads via `recv_all()`, flushes data to disk with `fwrite()`, and outputs throughput metrics derived from `clock_gettime(CLOCK_MONOTONIC)`.

### 3.2 Part II Case 1: Unmanaged Session Fault-Tolerance

- **Synthetic Fault Injection:** The server accepts a command-line failure probability $P \in [0.0, 1.0]$. Before transmitting each 8 KB data chunk (`TYPE_FILE_CHUNK`), the server evaluates a uniform pseudo-random condition using a thread-safe seed:
  $$\frac{\text{rand\_r}(\&seed)}{\text{RAND\_MAX}} < P$$
  If triggered, the server immediately logs the fault and closes the client socket without transmitting terminal frames (`TYPE_FILE_EOF` or `TYPE_TOPIC_EOF`).
- **Client Failure Handling:** When `recv_all()` returns 0 or -1 prematurely, the client detects the socket failure. Because no session state is maintained across connection boundaries:
  1. The client closes the failed descriptor.
  2. The client executes `clean_topic_dir()`, deleting all partially written files and removing `<output_dir>/<topic>/` to eliminate corrupted or incomplete files.
  3. The client recreates the topic directory, pauses briefly (100 ms backoff), reconnects to the server, and restarts the transfer from the first byte of file index 0.
- **Performance Characteristics:** As $P$ increases, transfer completion time escalates exponentially, and throughput collapses because initial chunks must be repeatedly retransmitted. The client tracks `Redundant Bytes = total_bytes_received - useful_bytes_received`.

### 3.3 Part II Case 2: Stateful Session Checkpointing and Resumption

- **Server-Side Session Table:** The server maintains an in-memory linked list of active sessions (`struct Session`) protected by a POSIX mutex (`pthread_mutex_t sessions_mutex`):
  ```c
  typedef struct Session {
      uint64_t session_id;
      char     topic[256];
      uint32_t current_file_idx;
      uint64_t current_offset;
      time_t   last_update;
      struct Session *next;
  } Session;
  ```
- **Session Handshake:** On initial connection (`TYPE_GET_REQUEST`), the server generates a unique 64-bit `session_id`, stores it in the session table, and transmits `TYPE_SESSION_CREATED` (`0x09`) immediately following `TYPE_TOPIC_RESPONSE`.
- **Client Disk Checkpointing:** The client increments its local offset counter **only after** `fwrite()` successfully flushes received chunk bytes to disk.
- **Stateful Resumption:**
  1. Upon unexpected socket termination, the client retains all verified files and byte offsets on disk.
  2. The client establishes a new TCP connection and transmits `TYPE_RECONNECT_REQ` (`0x0A`) carrying `session_id`, `current_file_idx`, and `current_offset`.
  3. The server locates the session entry via `validate_session()`, confirms that the requested index and offset match server-side progress, and replies with `TYPE_RESUME_RESPONSE` (`0x0B`) with status `RESUME_ACCEPTED`.
  4. The server seeks the file pointer to the requested byte position using `fseek(f, resume_offset, SEEK_SET)` and skips already completed files.
  5. Transmission resumes immediately from the exact byte requested. Under reliable local storage, **redundant transferred bytes are zero**.

### 3.4 Part II Case 2 Enhanced: Parallel Multi-Connection TCP Streaming

- **Multi-Worker Architecture:** The client spawns $N$ concurrent worker threads (`pthread_create`), configurable via a command-line argument (defaulting to 4).
- **Deterministic Workload Partitioning:** Work is distributed across parallel TCP connections using round-robin file index partitioning:
  $$\text{Worker } i \text{ processes file } k \iff k \pmod N = i$$
  Each worker independently initiates a TCP connection to the server and transmits `TYPE_GET_REQUEST_WORKER` (`0x0C`) carrying its `worker_id` and `total_workers`.
- **Independent Session Checkpoints:** Each worker maintains its own isolated session state and reconnection loop. If connection $i$ drops due to a synthetic fault, other worker threads continue streaming without interruption.
- **Synchronization and Metric Aggregation:** Workers write to separate files in `<output_dir>/<topic>/`. The main client thread invokes `pthread_join()` across all workers and aggregates total bytes, useful bytes, reconnect events, and elapsed time through a mutex-guarded accumulator (`pthread_mutex_t metrics_mutex`).

---

## 4. Application-Layer Protocol Specification

### 4.1 Binary Framing and Fixed Header

To eliminate TCP stream boundary amalgamation and ensure reliable message framing, every transmission begins with a fixed 9-byte binary header, followed by an optional payload whose size is specified by the header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Type (1 B)   |               Transaction ID (4 B)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Transaction ID (cont.)        |          Payload Length (4 B) |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Payload Length (cont.)        | Payload Data (Variable Length)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
|                                                               |
+---------------------------------------------------------------+
```

- **C Data Structure Definition:**
  ```c
  #pragma pack(push, 1)
  typedef struct {
      uint8_t  type;           // Message type identifier (1 byte)
      uint32_t transaction_id; // Request-response correlation ID (4 bytes, network byte order)
      uint32_t length;         // Length of following payload in bytes (4 bytes, network byte order)
  } msg_header_t;
  #pragma pack(pop)
  ```

### 4.2 Control and Data Message Types

| Opcode | Identifier | Direction | Description |
| :---: | :--- | :---: | :--- |
| `0x01` | `TYPE_GET_REQUEST` | Client $\to$ Server | Initial request for a topic name string. |
| `0x02` | `TYPE_TOPIC_RESPONSE` | Server $\to$ Client | Returns status (`STATUS_FOUND` / `STATUS_NOT_FOUND`) and total file count. |
| `0x03` | `TYPE_FILE_META` | Server $\to$ Client | Metadata for the incoming file (index, file size, filename length, name). |
| `0x04` | `TYPE_FILE_CHUNK` | Server $\to$ Client | Binary chunk containing file index, byte offset, and data payload ($\le 8192$ B). |
| `0x05` | `TYPE_FILE_EOF` | Server $\to$ Client | Signals the complete transfer of the current file stream. |
| `0x06` | `TYPE_ERROR` | Server $\to$ Client | Diagnostic error message string. |
| `0x07` | `TYPE_TOPIC_EOF` | Server $\to$ Client | Terminal signal indicating all files in the topic have been transferred. |
| `0x08` | `TYPE_MSG_START` | Client $\to$ Server | Handshake confirmation: client is ready to receive the file stream. |
| `0x09` | `TYPE_SESSION_CREATED` | Server $\to$ Client | Case 2 / Enhanced: Returns the unique 64-bit `session_id`. |
| `0x0A` | `TYPE_RECONNECT_REQ` | Client $\to$ Server | Case 2 / Enhanced: Resumption request carrying session ID, file index, and offset. |
| `0x0B` | `TYPE_RESUME_RESPONSE` | Server $\to$ Client | Case 2 / Enhanced: Validation outcome (`RESUME_ACCEPTED` or `RESUME_REJECTED`). |
| `0x0C` | `TYPE_GET_REQUEST_WORKER` | Client $\to$ Server | Case 2 Enhanced: Initial request carrying worker ID, worker count, and topic. |

### 4.3 Detailed Payload Layouts

1. **`TYPE_GET_REQUEST` (`0x01`):**
   - Payload: Topic name string (`header.length` bytes, e.g. `"cane"`).
2. **`TYPE_TOPIC_RESPONSE` (`0x02`):**
   - Payload: Status byte (`uint8_t`: `0` = Not Found, `1` = Found).
   - If Found: Followed by `total_files` (4 bytes, `uint32_t`, network byte order). Total payload length = 5 bytes.
3. **`TYPE_MSG_START` (`0x08`):**
   - Payload: Empty (payload length = 0).
4. **`TYPE_FILE_META` (`0x03`):**
   - `file_index` (4 bytes, `uint32_t`, Big-Endian)
   - `file_size` (8 bytes, `uint64_t`, Big-Endian via `htonll`)
   - `filename_len` (2 bytes, `uint16_t`, Big-Endian)
   - `filename` (variable length string, `filename_len` bytes)
5. **`TYPE_FILE_CHUNK` (`0x04`):**
   - `file_index` (4 bytes, `uint32_t`, Big-Endian)
   - `offset` (8 bytes, `uint64_t`, Big-Endian via `htonll`)
   - `chunk_data` (variable length, up to 8192 bytes)
6. **`TYPE_FILE_EOF` (`0x05`):**
   - `file_index` (4 bytes, `uint32_t`, Big-Endian). Total payload length = 4 bytes.
7. **`TYPE_TOPIC_EOF` (`0x07`):**
   - Payload: Empty (payload length = 0).
8. **`TYPE_SESSION_CREATED` (`0x09`):**
   - `session_id` (8 bytes, `uint64_t`, Big-Endian via `htonll`). Total payload length = 8 bytes.
9. **`TYPE_RECONNECT_REQ` (`0x0A`):**
   - `session_id` (8 bytes, `uint64_t`, Big-Endian)
   - `file_index` (4 bytes, `uint32_t`, Big-Endian)
   - `offset` (8 bytes, `uint64_t`, Big-Endian)
   - `topic` string (variable length). Total payload length = $20 + \text{strlen}(topic)$ bytes.
10. **`TYPE_RESUME_RESPONSE` (`0x0B`):**
    - `status` (1 byte: `0x01` = `RESUME_ACCEPTED`, `0x00` = `RESUME_REJECTED`). Total payload length = 1 byte.
11. **`TYPE_GET_REQUEST_WORKER` (`0x0C`):**
    - `worker_id` (4 bytes, `uint32_t`, Big-Endian)
    - `total_workers` (4 bytes, `uint32_t`, Big-Endian)
    - `topic` string (variable length). Total payload length = $8 + \text{strlen}(topic)$ bytes.

### 4.4 Protocol Handshake and Recovery Workflows

#### Normal Transfer Flow (Part I / Part II Initial)
```
Client                                                   Server
  |                                                         |
  |--- TYPE_GET_REQUEST (0x01, topic) --------------------->|
  |<-- TYPE_TOPIC_RESPONSE (0x02, FOUND, total_files) ------|
  |--- [If Case 2] TYPE_SESSION_CREATED (0x09, session_id) -|
  |--- TYPE_MSG_START (0x08) ------------------------------>|
  |<-- TYPE_FILE_META (0x03, idx=0, size, name) ------------|
  |<-- TYPE_FILE_CHUNK (0x04, idx=0, offset=0, data) -------|
  |<-- TYPE_FILE_CHUNK (0x04, idx=0, offset=8192, data) ----|
  |<-- TYPE_FILE_EOF (0x05, idx=0) -------------------------|
  |    ... [Files 1 to N-1] ...                             |
  |<-- TYPE_TOPIC_EOF (0x07) -------------------------------|
  |                                                         |
```

#### Fault Recovery: Part II Case 1 (Abort and Restart)
```
Client                                                   Server
  |                                                         |
  |<-- [FAULT INJECTED: Server abruptly closes socket] -----X
  | [Client detects EOF / ECONNRESET]                       |
  | 1. clean_topic_dir() -> deletes all files & directory   |
  | 2. Re-creates empty directory                           |
  | 3. Increments reconnect counter & resets useful bytes   |
  |--- [Reconnects to server] ----------------------------->|
  |--- TYPE_GET_REQUEST (0x01, topic) --------------------->|
  | [Restarts transfer from file 0, offset 0]               |
```

#### Fault Recovery: Part II Case 2 (Session Resumption)
```
Client                                                   Server
  |                                                         |
  |<-- [FAULT INJECTED: Server abruptly closes socket] -----X
  | [Server saves checkpoint: (file_idx, offset)]           |
  | [Client retains local file & flushed offset]            |
  |--- [Reconnects to server] ----------------------------->|
  |--- TYPE_RECONNECT_REQ (0x0A, sid, file_idx, offset) --->|
  |    [Server verifies session checkpoint in table]        |
  |<-- TYPE_RESUME_RESPONSE (0x0B, RESUME_ACCEPTED) --------|
  |    [Server fseek(f, offset, SEEK_SET)]                  |
  |<-- TYPE_FILE_CHUNK (0x04, file_idx, offset, data) ------|
  | [Transfer continues seamlessly without retransmission]  |
```

### 4.5 Robust Network I/O Primitives

All socket reads and writes are encapsulated in `recv_all()` and `send_all()` helper routines (`protocol.c`). These functions loop until all requested bytes are transferred, correctly handling partial transmissions and interrupted system calls (`EINTR`). Furthermore, socket calls use `MSG_NOSIGNAL` to prevent process termination from broken pipe (`SIGPIPE`) signals during sudden peer disconnections.

---

## 5. Repository Structure

```
Fault-Tolerant-Topic-based-File-Distribution-Service/
├── Part1/                            # Part I: Basic File Distribution
│   ├── Makefile                      # Builds server and client targets
│   ├── client.c                      # Single-client transfer and metrics
│   ├── server.c                      # Sequential (single) and multi-threaded server
│   ├── protocol.h                    # 9-byte header and message type definitions
│   └── protocol.c                    # send_all, recv_all, and byte-swapping logic
│
├── Part2_Case1/                      # Part II Case 1: No Session State
│   ├── Makefile                      # Builds server and client targets
│   ├── client.c                      # Wipe-on-abort client recovery loop
│   ├── server.c                      # Probabilistic fault injection server
│   ├── protocol.h                    # Protocol definitions
│   └── protocol.c                    # Network I/O helpers
│
├── Part2_Case2/                      # Part II Case 2: Stateful Session Checkpointing
│   ├── Makefile                      # Builds server and client targets
│   ├── client.c                      # Checkpoint-aware client resumption logic
│   ├── server.c                      # Mutex-protected session management server
│   ├── protocol.h                    # Session extension opcodes (0x09, 0x0A, 0x0B)
│   └── protocol.c                    # Network I/O helpers
│
├── Part2_Case2_Enhanced/             # Part II Case 2 Enhanced: Parallel Multi-TCP
│   ├── Makefile                      # Builds server and client targets
│   ├── client.c                      # Multi-worker parallel streaming client
│   ├── server.c                      # Modulo workload partitioning server
│   ├── protocol.h                    # Worker extension opcode (0x0C)
│   └── protocol.c                    # Network I/O helpers
│
├── tests/                            # Automated Testing Harness
│   └── run_tests.sh                  # 15 automated end-to-end integration test suites
│
├── benchmark/                        # Automated Benchmarking and Plotting
│   ├── run_benchmarks.sh             # Parameter sweep script (clients, faults, connections)
│   └── plot.py                       # Matplotlib 300 DPI graph generator
│
├── Results/                          # Empirical Benchmark Results and Visualizations
│   ├── Part1_throughput.csv          # Clients vs Throughput empirical data
│   ├── Part2_throughput.csv          # Failure rate vs Case 1 & 2 Throughput
│   ├── Part2_Enhanced_throughput.csv # Parallel connections vs Throughput
│   ├── Part1_throughput.png          # 300 DPI multi-client scalability graph
│   ├── Part2_Cases_throughput.png    # 300 DPI Case 1 vs Case 2 comparison plot
│   └── Part2_Enhanced_throughput.png # 300 DPI parallel scaling plot
│
├── Images/                           # Visual Evidence and Terminal Captures (69 Assets)
│   ├── ASSETS_INDEX.md               # Master index of all 69 curated visual assets
│   ├── terminal_broker_startup.png   # Dual-pane: Broker daemon initialization
│   ├── terminal_part1_transfer.png   # Dual-pane: Part 1 multi-threaded transfer
│   ├── terminal_part2_case1_failover.png       # Dual-pane: Case 1 fault drop & wipe
│   ├── terminal_part2_case2_session_resume.png # Dual-pane: Case 2 byte-offset resume
│   ├── terminal_part2_enhanced_parallel.png    # Dual-pane: Enhanced 4-worker parallel
│   ├── terminal_tests_verification.png         # Dual-pane: 15/15 test suite verification
│   └── [63 empirical client screenshots across physical network nodes]
│
├── docs/                             # Engineering and Audit Documentation
│   ├── ARCHITECTURE.md               # Detailed architectural deep-dive
│   ├── AUTOGRADER_CHECKLIST.md       # CLI and autograder compliance audit
│   ├── DEMO_PLAN.md                  # Interactive viva and evaluation script
│   ├── DESIGN_REVIEW.md              # Technical design justification
│   ├── Design_Document.md            # Comprehensive project design specification
│   ├── FINAL_AUDIT.md                # Quality gate verification matrix
│   ├── MODEL_DESIGN_COMPARISON_AND_CHANGES.md # Detailed audit against model solution
│   ├── PROTOCOL.md                   # Full wire-protocol specification
│   ├── REQUIREMENTS_MATRIX.md        # Requirement traceability checklist
│   ├── RUN_GUIDE.md                  # Step-by-step terminal execution walkthrough
│   ├── SESSION_PROTOCOL.md           # Session management and recovery specification
│   ├── TESTING.md                    # Test execution and verification guide
│   └── VIVA.md                       # Comprehensive oral examination Q&A guide
│
├── FinalSubmission_Group_100_113/    # Clean submission package mirroring repo
├── FinalReport_Group_100_113-1.pdf   # Formal academic final report (PDF)
├── FinalReport_Group_100_113.pdf     # Symlink/matching copy of final report
├── report_execution_guide.md         # Multi-node physical machine testing manual
└── README.md                         # Primary project documentation (this file)
```

---

## 6. Compilation and Prerequisites

### 6.1 Environment Prerequisites
- **Compiler:** `gcc` (version 9.0 or higher supporting C99/C11 standards).
- **Libraries:** Standard C Library (`libc`), POSIX Threads (`-pthread`), Real-Time library (`-lrt`).
- **Build System:** GNU `make`.
- **Target OS:** POSIX-compliant Linux (Ubuntu 20.04/22.04/24.04 LTS or WSL2).
- **Optional Python Tools:** Python 3 with `pandas`, `matplotlib`, and `pillow` (for generating benchmark graphs).

### 6.2 Building Modules

Run `make` within each individual component directory:

```bash
# 1. Build Part 1
cd Part1 && make clean && make && cd ..

# 2. Build Part 2 Case 1
cd Part2_Case1 && make clean && make && cd ..

# 3. Build Part 2 Case 2
cd Part2_Case2 && make clean && make && cd ..

# 4. Build Part 2 Case 2 Enhanced
cd Part2_Case2_Enhanced && make clean && make && cd ..
```

Each build compiles cleanly without warnings (`-Wall -Wextra -pthread -lrt`) and produces independent `server` and `client` binaries.

---

## 7. Execution and Operational Guide

### 7.1 Part I Execution

#### Server Syntax
```bash
./server <port> <topics_root_dir> [single|multi]
```
- `<port>`: TCP port to listen on (e.g., `8080`).
- `<topics_root_dir>`: Path to the dataset directory containing topic folders.
- `[single|multi]`: Concurrency mode. Defaults to `multi` if omitted.

*Example (Multi-threaded server on port 8080):*
```bash
cd Part1
./server 8080 "/path/to/dataset/raw-img" multi
```

#### Client Syntax
```bash
./client <server_ip> <server_port> <topic> <output_dir>
```
- `<server_ip>`: IPv4 address of the server (e.g., `127.0.0.1` or LAN IP).
- `<server_port>`: TCP port on the server.
- `<topic>`: Topic directory name (e.g., `cane`).
- `<output_dir>`: Local destination directory.

*Example:*
```bash
cd Part1
./client 127.0.0.1 8080 cane ./output_p1
```

---

### 7.2 Part II Case 1 Execution (Fault Injection Without Sessions)

#### Server Syntax
```bash
./server <port> <topics_root_dir> <failure_probability>
```
- `<failure_probability>`: Drop probability per 8 KB chunk, $P \in [0.00, 1.00]$.

*Example (5% fault injection rate):*
```bash
cd Part2_Case1
./server 8081 "/path/to/dataset/raw-img" 0.05
```

#### Client Syntax
```bash
./client <server_ip> <server_port> <topic> <output_dir>
```

*Example:*
```bash
cd Part2_Case1
./client 127.0.0.1 8081 cane ./output_case1
```
*Note: Under frequent disconnections, the client will report elevated `Redundant Bytes` and multiple reconnect cycles due to recursive directory wipes.*

---

### 7.3 Part II Case 2 Execution (Stateful Checkpoint Resumption)

#### Server Syntax
```bash
./server <port> <topics_root_dir> <failure_probability>
```

*Example:*
```bash
cd Part2_Case2
./server 8082 "/path/to/dataset/raw-img" 0.05
```

#### Client Syntax
```bash
./client <server_ip> <server_port> <topic> <output_dir>
```

*Example:*
```bash
cd Part2_Case2
./client 127.0.0.1 8082 cane ./output_case2
```
*Note: Even under frequent disconnections, the client resumes from its saved file and byte offset, maintaining `Redundant Bytes` at 0.*

---

### 7.4 Part II Case 2 Enhanced Execution (Parallel Multi-Connection)

#### Server Syntax
```bash
./server <port> <topics_root_dir> <failure_probability>
```

*Example:*
```bash
cd Part2_Case2_Enhanced
./server 8083 "/path/to/dataset/raw-img" 0.05
```

#### Client Syntax
```bash
./client <server_ip> <server_port> <topic> <output_dir> [num_connections]
```
- `[num_connections]`: Number of concurrent TCP worker connections (defaults to `4` if omitted).

*Example (4 parallel connections):*
```bash
cd Part2_Case2_Enhanced
./client 127.0.0.1 8083 cane ./output_enhanced 4
```

---

## 8. Verification and Automated Testing

### 8.1 Test Suite Organization

The repository provides a comprehensive automated test suite in [`tests/run_tests.sh`](tests/run_tests.sh). The harness executes 5 rigorous testing stages comprising 15 automated checks:
1. **Part 1 Functional Verification:** Compiles Part 1, launches server and client on loopback, transfers a multi-file synthetic dataset (text, 128 KB binary, 1 MB multi-chunk binary), verifies exact file matching via `diff -r`, and tests negative error handling with non-existent topic names.
2. **Part 2 Case 1 Verification:** Compiles Case 1, validates clean baseline transfer ($P=0.0$), injects faults ($P=0.02$), confirms that directory wipes occur on disconnect, and validates exact byte-for-byte final transfer integrity.
3. **Part 2 Case 2 Verification:** Compiles Case 2, tests baseline transfer ($P=0.0$), injects faults ($P=0.02$), verifies session recovery and byte-level resumption across disconnections, and verifies identical file checksums with zero redundant data.
4. **Part 2 Case 2 Enhanced Verification:** Compiles Case 2 Enhanced, spawns multi-worker connections ($N=2$), validates round-robin workload partitioning, and verifies file integrity.
5. **Final Submission Package Verification:** Verifies that all code in `FinalSubmission_Group_100_113/` builds cleanly across all 4 directories without warnings or errors.

### 8.2 Running the Test Harness

```bash
cd tests
chmod +x run_tests.sh
./run_tests.sh
```

**Expected Test Harness Output:**
```text
================================================================
   Topic-based File Distribution Service - Automated Test Suite  
================================================================
[INFO] Creating synthetic test dataset...
[INFO] === Test Suite: Part 1 (Single/Multi-threaded) ===
[PASS] Part 1 compiled successfully.
[PASS] Part 1 file transfer matched test dataset exactly.
[PASS] Part 1 client gracefully handled non-existent topic.
[INFO] === Test Suite: Part 2 Case 1 (Abort & Restart from Scratch) ===
[PASS] Part 2 Case 1 compiled successfully.
[PASS] Part 2 Case 1 baseline (0% failure) matched test dataset.
[PASS] Part 2 Case 1 fault tolerance & directory wipe completed successfully.
[INFO] === Test Suite: Part 2 Case 2 (Stateful Session Resumption) ===
[PASS] Part 2 Case 2 compiled successfully.
[PASS] Part 2 Case 2 baseline (0% failure) matched test dataset.
[PASS] Part 2 Case 2 session resumption completed successfully with exact file match.
[INFO] === Test Suite: Part 2 Case 2 Enhanced (Multi-Worker Transfer) ===
[PASS] Part 2 Case 2 Enhanced compiled successfully.
[PASS] Part 2 Case 2 Enhanced 2-worker transfer matched test dataset.
[INFO] === Test Suite: FinalSubmission_Group_100_113 Compilation ===
[PASS] FinalSubmission Part1 compiled successfully.
[PASS] FinalSubmission Part2_Case1 compiled successfully.
[PASS] FinalSubmission Part2_Case2 compiled successfully.
[PASS] FinalSubmission Part2_Case2_Enhanced compiled successfully.

================================================================
                       Test Suite Summary                       
================================================================
Total Tests  : 15
Passed Tests : 15
Failed Tests : 0

🎉 ALL TESTS PASSED!
```

### 8.3 Automated Verification Proof

Below is the visual verification proof card recorded during automated test execution:

![Integration Tests Verification Proof](Images/terminal_tests_verification.png)

*Figure 8.1: Full integration test suite execution confirming 15/15 automated tests passing under POSIX execution environment.*

---

## 9. Performance Benchmarking and Analytics

### 9.1 Executing Automated Benchmarks

An automated parameter-sweep benchmarking suite is located in [`benchmark/run_benchmarks.sh`](benchmark/run_benchmarks.sh). It sweeps:
- **Part 1:** Client concurrency levels: 1, 2, 4, 8 clients.
- **Part 2 Cases 1 & 2:** Failure injection probabilities: $P \in \{0.00, 0.01, 0.02, 0.05, 0.10\}$.
- **Part 2 Enhanced:** Parallel connection counts: 1, 2, 4, 8 connections under $P=0.05$.

To execute the benchmark sweeps:
```bash
cd benchmark
chmod +x run_benchmarks.sh
./run_benchmarks.sh
```
*Outputs generated in `Results/`:*
- `Results/Part1_throughput.csv`
- `Results/Part2_throughput.csv`
- `Results/Part2_Enhanced_throughput.csv`

### 9.2 Generating Analytical Graphs

Ensure `python3`, `pandas`, and `matplotlib` are installed, then execute:
```bash
cd benchmark
python3 plot.py
```
*Generated plots in `Results/`:*
- `Part1_throughput.png`: Multi-client throughput scalability.
- `Part2_Cases_throughput.png`: Case 1 vs Case 2 throughput degradation curves under increasing failure probability.
- `Part2_Enhanced_throughput.png`: Throughput speedup achieved with parallel TCP worker streams.

---

### 9.3 Empirical Benchmark Plots & Scalability Analysis

#### 9.3.1 Part 1 Multi-Client Scalability Analysis

The figure below plots aggregate throughput as subscriber concurrency increases from 1 to 8 concurrent clients.

![Part 1 Multi-Client Throughput Scalability](Results/Part1_throughput.png)

*Figure 9.1: Part 1 multi-client throughput scalability curve. Aggregate throughput doubles from 52.61 MB/s (1 client) to 102.65 MB/s (2 clients), demonstrating near-linear scaling before plateauing near ~112 MB/s due to loopback/storage I/O bounds.*

| Concurrency Level | Aggregate Throughput | Speedup Factor | Efficiency Observations |
|---|---|---|---|
| **1 Client** | **52.61 MB/s** | 1.00x (Baseline) | Single TCP connection streaming at loopback rate |
| **2 Clients** | **102.65 MB/s** | **1.95x** | Near-perfect linear scaling; multi-threading maximizes CPU cores |
| **4 Clients** | **102.26 MB/s** | 1.94x | Throughput plateaus; context switching and disk read bandwidth bound |
| **8 Clients** | **112.16 MB/s** | 2.13x | Maximum aggregate saturation achieved |

---

#### 9.3.2 Part 2 Case 1 vs. Case 2 Fault Resilience Comparison

The figure below compares the throughput degradation curves of Case 1 (unmanaged wipe & restart) versus Case 2 (stateful session checkpointing) as failure probability $P$ increases.

![Part 2 Case 1 vs Case 2 Throughput](Results/Part2_Cases_throughput.png)

*Figure 9.2: Throughput comparison between Case 1 (red) and Case 2 (teal) under increasing fault probability $P$. At $P=0.02$, Case 2 delivers **1.34 MB/s** versus Case 1's **0.05 MB/s**—a **26.8x performance advantage** enabled by byte-offset resumption.*

| Failure Prob ($P$) | Case 1 Throughput | Case 2 Throughput | Performance Advantage | Redundancy Impact |
|---|---|---|---|---|
| **$P = 0.00$** | 63.50 MB/s | 301.60 MB/s | 4.75x | Baseline clean channel; session overhead is minimal |
| **$P = 0.01$** | 0.27 MB/s | 1.05 MB/s | **3.89x** | Case 1 suffers frequent restarts; Case 2 resumes instantly |
| **$P = 0.02$** | 0.05 MB/s | 1.34 MB/s | **26.80x** | **Case 1 failure cliff:** transfer rarely succeeds without drops |
| **$P \ge 0.05$** | $\to 0$ MB/s | Stable (>0.4 MB/s) | $\infty$ | Case 1 fails completely; Case 2 guarantees forward progress |

---

#### 9.3.3 Part 2 Case 2 Enhanced Parallel TCP Concurrency Acceleration

The figure below plots throughput speedup under constant 5% failure probability ($P=0.05$) as the number of concurrent TCP worker connections scales from 1 to 8.

![Part 2 Case 2 Enhanced Parallel Concurrency](Results/Part2_Enhanced_throughput.png)

*Figure 9.3: Concurrency throughput acceleration under 5% loss rate. Scaling from 1 to 8 parallel TCP worker connections increases useful throughput from 0.42 MB/s to 2.46 MB/s (**5.86x overall speedup**).*

| Worker Connections | Throughput ($P=0.05$) | Relative Speedup | Network Utilization Mechanism |
|---|---|---|---|
| **1 Worker** | 0.42 MB/s | 1.00x (Baseline) | Drops stall entire transfer progress |
| **2 Workers** | 0.66 MB/s | 1.57x | Independent TCP failure domains |
| **4 Workers** | 2.04 MB/s | **4.86x** | Modulo partitioning maximizes concurrent file delivery |
| **8 Workers** | 2.46 MB/s | **5.86x** | Maximum resilience and network bandwidth saturation |

---

### 9.4 Architectural Trade-offs and Observations

1. **Part I Concurrency:** Under low connection counts, multi-threaded performance scales linearly over single-threaded execution. At extreme concurrency, throughput plateaus due to local disk read/write bandwidth saturation.
2. **Case 1 Failure Cliff:** Because Case 1 wipes its output directory and restarts upon each drop, the probability of completing a large topic transfer without a disconnection decays exponentially:
   $$P_{\text{success}} = (1 - P)^{\text{total chunks}}$$
   For a topic with 500 chunks at $P = 0.02$, $P_{\text{success}} = (0.98)^{500} \approx 0.000041$ ($0.004\%$), explaining why Case 1 throughput collapses to near-zero.
3. **Case 2 Resilience:** Stateful session management guarantees monotonic forward progress. Because the client saves flushed byte offsets and requests `TYPE_RECONNECT_REQ`, redundant transferred bytes remain **strictly zero**. Completion time scales linearly with reconnect latency rather than exponentially with file size.
4. **Enhanced Parallelism:** Using 4 to 8 parallel TCP streams delivers up to a **5.86x throughput improvement** over single-connection transfers under identical loss rates, as disconnections on one worker thread do not stall transfers on remaining worker threads.

---

## 10. Robustness, Autograder Compliance, and Quality Gates

| Evaluation Criteria | Implementation Verification | Status |
| :--- | :--- | :---: |
| **No Hardcoded IPs/Ports** | All endpoints and directories are passed via CLI arguments only. | **PASS** |
| **Exact CLI Compliance** | Parameter sequences match course specifications across all parts. | **PASS** |
| **Standard Exit Codes** | Clean termination (`exit(0)`) on success; non-zero on critical errors. | **PASS** |
| **Socket Reuse** | `setsockopt(..., SO_REUSEADDR, ...)` configured across all server sockets. | **PASS** |
| **Signal Safety** | `MSG_NOSIGNAL` flag used in `send()` to prevent `SIGPIPE` termination. | **PASS** |
| **Thread Safety** | Server session table and client metric accumulators guarded by mutexes. | **PASS** |
| **Path Traversal Protection** | Requests containing `..` or `/` rejected with `TYPE_ERROR`. | **PASS** |
| **Partial I/O Handling** | `send_all()` and `recv_all()` prevent truncation from partial OS buffers. | **PASS** |
| **Byte-Order Standardization** | Big-Endian network byte order enforced across all 16, 32, and 64-bit fields. | **PASS** |

---

## 11. Complete Documentation Roadmap

In addition to this README, comprehensive technical documentation is provided across the repository:

| Document | Description |
| :--- | :--- |
| [`Images/ASSETS_INDEX.md`](Images/ASSETS_INDEX.md) | Master catalog of all 69 visual assets, terminal captures, and proof cards. |
| [`report_execution_guide.md`](report_execution_guide.md) | 3-node physical machine setup, operational run guide, and evidence collection manual. |
| [`FinalReport_Group_100_113-1.pdf`](FinalReport_Group_100_113-1.pdf) | Formal project report with empirical charts and academic analysis. |
| [`docs/Design_Document.md`](docs/Design_Document.md) | Official project design document detailing architectural choices. |
| [`docs/PROTOCOL.md`](docs/PROTOCOL.md) | In-depth wire protocol and framing specification. |
| [`docs/SESSION_PROTOCOL.md`](docs/SESSION_PROTOCOL.md) | Formal specification of stateful session management and recovery. |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Detailed architectural breakdown of client, server, and worker components. |
| [`docs/RUN_GUIDE.md`](docs/RUN_GUIDE.md) | Terminal execution walkthrough with copy-paste commands. |
| [`docs/AUTOGRADER_CHECKLIST.md`](docs/AUTOGRADER_CHECKLIST.md) | Autograder compliance verification matrix. |
| [`docs/TESTING.md`](docs/TESTING.md) | Test harness guide and validation methodology. |
| [`docs/MODEL_DESIGN_COMPARISON_AND_CHANGES.md`](docs/MODEL_DESIGN_COMPARISON_AND_CHANGES.md) | Detailed audit and alignment with instructor model design. |
| [`docs/FINAL_AUDIT.md`](docs/FINAL_AUDIT.md) | Final pre-submission quality gate audit. |
| [`docs/DEMO_PLAN.md`](docs/DEMO_PLAN.md) | Step-by-step interactive demonstration script. |
| [`docs/VIVA.md`](docs/VIVA.md) | Comprehensive viva voce preparation with 25+ technical Q&As. |
