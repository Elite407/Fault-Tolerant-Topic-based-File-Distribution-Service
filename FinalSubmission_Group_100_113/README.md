# Fault-Tolerant Topic-Based Binary File Distribution Service

**Course:** IT305: Computer Networks Laboratory  
**Institution:** Dhirubhai Ambani University (Formerly DA-IICT)  
**Project Group:** Group 100 & Group 113  
**Technology Stack:** C (C99/C11 standards), POSIX BSD Sockets, POSIX Threads (`pthreads`), Real-Time Extensions (`librt`)

---

## Table of Contents
- [1. Executive Summary](#1-executive-summary)
- [2. System Architecture](#2-system-architecture)
  - [2.1 Part I: Basic File Distribution Service](#21-part-i-basic-file-distribution-service)
  - [2.2 Part II Case 1: Unmanaged Session Fault-Tolerance](#22-part-ii-case-1-unmanaged-session-fault-tolerance)
  - [2.3 Part II Case 2: Stateful Session Checkpointing and Resumption](#23-part-ii-case-2-stateful-session-checkpointing-and-resumption)
  - [2.4 Part II Case 2 Enhanced: Parallel Multi-Connection TCP Streaming](#24-part-ii-case-2-enhanced-parallel-multi-connection-tcp-streaming)
- [3. Application-Layer Protocol Specification](#3-application-layer-protocol-specification)
  - [3.1 Binary Framing and Fixed Header](#31-binary-framing-and-fixed-header)
  - [3.2 Control and Data Message Types](#32-control-and-data-message-types)
  - [3.3 Detailed Payload Layouts](#33-detailed-payload-layouts)
  - [3.4 Protocol Handshake and Recovery Workflows](#34-protocol-handshake-and-recovery-workflows)
  - [3.5 Robust Network I/O Primitives](#35-robust-network-io-primitives)
- [4. Repository Structure](#4-repository-structure)
- [5. Compilation and Prerequisites](#5-compilation-and-prerequisites)
  - [5.1 Environment Prerequisites](#51-environment-prerequisites)
  - [5.2 Building Modules](#52-building-modules)
- [6. Execution and Operational Guide](#6-execution-and-operational-guide)
  - [6.1 Part I Execution](#61-part-i-execution)
  - [6.2 Part II Case 1 Execution](#62-part-ii-case-1-execution)
  - [6.3 Part II Case 2 Execution](#63-part-ii-case-2-execution)
  - [6.4 Part II Case 2 Enhanced Execution](#64-part-ii-case-2-enhanced-execution)
- [7. Verification and Automated Testing](#7-verification-and-automated-testing)
  - [7.1 Test Suite Organization](#71-test-suite-organization)
  - [7.2 Running the Test Harness](#72-running-the-test-harness)
- [8. Performance Benchmarking and Analytics](#8-performance-benchmarking-and-analytics)
  - [8.1 Executing Automated Benchmarks](#81-executing-automated-benchmarks)
  - [8.2 Generating Analytical Graphs](#82-generating-analytical-graphs)
  - [8.3 Empirical Observations and Trade-offs](#83-empirical-observations-and-trade-offs)
- [9. Robustness, Autograder Compliance, and Quality Gates](#9-robustness-autograder-compliance-and-quality-gates)
- [10. Complete Documentation Roadmap](#10-complete-documentation-roadmap)

---

## 1. Executive Summary

This project implements a high-performance, fault-tolerant, topic-based binary file distribution service in C using POSIX BSD sockets and POSIX threads (`pthreads`). Operating on an application-level client-server paradigm, datasets are organized hierarchically by topic names (e.g., animal classes from the Animals-10 dataset). Clients issue structured binary requests for specific topics, and the server transmits all associated files using a custom framing protocol that guarantees complete boundary preservation across stream-oriented TCP sockets.

The system addresses both foundational networking requirements and complex fault-tolerance challenges across four distinct operational modes:
1. **Part I:** Single-threaded (sequential) and multi-threaded (concurrent listener-dispatcher) file transfer.
2. **Part II Case 1:** Fault-injected environment without session state, enforcing full transfer aborts, recursive directory wipes, and clean restarts from scratch upon disconnection.
3. **Part II Case 2:** Fault-injected environment featuring thread-safe stateful session management, verified disk-write checkpoints, and resumption from the exact byte offset where the fault occurred (achieving 0 bytes of redundant data retransmission).
4. **Part II Case 2 Enhanced:** Parallel multi-connection TCP streaming across concurrent client worker threads, deterministically partitioning files via modulo arithmetic and isolating failure domains.

---

## 2. System Architecture

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

### 2.1 Part I: Basic File Distribution Service

- **Startup Discovery:** At startup, the server inspects the provided `<topics_root_dir>` via `init_topic_table()`. It iterates through directory entries, skips hidden files (`.` and `..`), validates read and execute access permissions (`access(R_OK | X_OK)`), and populates an in-memory `TopicEntry topic_table[MAX_TOPICS]`.
- **Single-Threaded Execution:** The server processes client requests sequentially. Incoming connections queue in the OS TCP listen backlog (`listen(..., 100)`). The server performs the entire handshake, streams each file sequentially, sends `TYPE_TOPIC_EOF`, closes the socket, and only then invokes `accept()` for the next client.
- **Multi-Threaded Execution:** The main thread functions solely as an event listener. Upon `accept()`, it allocates a client context structure on the heap and spawns a detached POSIX thread (`pthread_create` + `pthread_detach`). Multiple clients execute file reading and transmission simultaneously without blocking each other.
- **Client Pipeline:** The client sends `TYPE_GET_REQUEST`, verifies `TYPE_TOPIC_RESPONSE`, issues `TYPE_MSG_START`, creates `<output_dir>/<topic>/` recursively via `mkdir_p()`, receives chunk payloads via `recv_all()`, flushes data to disk with `fwrite()`, and outputs throughput metrics derived from `clock_gettime(CLOCK_MONOTONIC)`.

### 2.2 Part II Case 1: Unmanaged Session Fault-Tolerance

- **Synthetic Fault Injection:** The server accepts a command-line failure probability $P \in [0.0, 1.0]$. Before transmitting each 8 KB data chunk (`TYPE_FILE_CHUNK`), the server evaluates a uniform pseudo-random condition using a thread-safe seed:
  $$\frac{\text{rand\_r}(\&seed)}{\text{RAND\_MAX}} < P$$
  If triggered, the server immediately logs the fault and closes the client socket without transmitting terminal frames (`TYPE_FILE_EOF` or `TYPE_TOPIC_EOF`).
- **Client Failure Handling:** When `recv_all()` returns 0 or -1 prematurely, the client detects the socket failure. Because no session state is maintained across connection boundaries:
  1. The client closes the failed descriptor.
  2. The client executes `clean_topic_dir()`, deleting all partially written files and removing `<output_dir>/<topic>/` to eliminate corrupted or incomplete files.
  3. The client recreates the topic directory, pauses briefly (100 ms backoff), reconnects to the server, and restarts the transfer from the first byte of file index 0.
- **Performance Characteristics:** As $P$ increases, transfer completion time escalates exponentially, and throughput collapses because initial chunks must be repeatedly retransmitted. The client tracks `Redundant Bytes = total_bytes_received - useful_bytes_received`.

### 2.3 Part II Case 2: Stateful Session Checkpointing and Resumption

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
- **Session Handshake:** On initial connection (`TYPE_GET_REQUEST`), the server generates a unique 64-bit `session_id`, stores it in the session table, and transmits `TYPE_SESSION_CREATED` (0x09) immediately following `TYPE_TOPIC_RESPONSE`.
- **Client Disk Checkpointing:** The client increments its local offset counter **only after** `fwrite()` successfully flushes received chunk bytes to disk.
- **Stateful Resumption:**
  1. Upon unexpected socket termination, the client retains all verified files and byte offsets on disk.
  2. The client establishes a new TCP connection and transmits `TYPE_RECONNECT_REQ` (0x0A) carrying `session_id`, `current_file_idx`, and `current_offset`.
  3. The server locates the session entry via `validate_session()`, confirms that the requested index and offset match server-side progress, and replies with `TYPE_RESUME_RESPONSE` (0x0B) with status `RESUME_ACCEPTED`.
  4. The server seeks the file pointer to the requested byte position using `fseek(f, resume_offset, SEEK_SET)` and skips already completed files.
  5. Transmission resumes immediately from the exact byte requested. Under reliable local storage, **redundant transferred bytes are zero**.

### 2.4 Part II Case 2 Enhanced: Parallel Multi-Connection TCP Streaming

- **Multi-Worker Architecture:** The client spawns $N$ concurrent worker threads (`pthread_create`), configurable via a command-line argument (defaulting to 4).
- **Deterministic Workload Partitioning:** Work is distributed across parallel TCP connections using round-robin file index partitioning:
  $$\text{Worker } i \text{ processes file } k \iff k \pmod N = i$$
  Each worker independently initiates a TCP connection to the server and transmits `TYPE_GET_REQUEST_WORKER` (0x0C) carrying its `worker_id` and `total_workers`.
- **Independent Session Checkpoints:** Each worker maintains its own isolated session state and reconnection loop. If connection $i$ drops due to a synthetic fault, other worker threads continue streaming without interruption.
- **Synchronization and Metric Aggregation:** Workers write to separate files in `<output_dir>/<topic>/`. The main client thread invokes `pthread_join()` across all workers and aggregates total bytes, useful bytes, reconnect events, and elapsed time through a mutex-guarded accumulator (`pthread_mutex_t metrics_mutex`).

---

## 3. Application-Layer Protocol Specification

### 3.1 Binary Framing and Fixed Header

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

### 3.2 Control and Data Message Types

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

### 3.3 Detailed Payload Layouts

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

### 3.4 Protocol Handshake and Recovery Workflows

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

### 3.5 Robust Network I/O Primitives

All socket reads and writes are encapsulated in `recv_all()` and `send_all()` helper routines (`protocol.c`). These functions loop until all requested bytes are transferred, correctly handling partial transmissions and interrupted system calls (`EINTR`). Furthermore, socket calls use `MSG_NOSIGNAL` to prevent process termination from broken pipe (`SIGPIPE`) signals during sudden peer disconnections.

---

## 4. Repository Structure

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
│   └── run_tests.sh                  # 5 end-to-end integration test suites
│
├── benchmark/                        # Automated Benchmarking and Plotting
│   ├── run_benchmarks.sh             # Parameter sweep script (clients, faults, connections)
│   └── plot.py                       # Matplotlib graph generator
│
├── Results/                          # Benchmark Results and Visualizations
│   ├── Part1_throughput.csv          # Clients vs Throughput data
│   ├── Part2_throughput.csv          # Failure rate vs Case 1 & 2 Throughput
│   ├── Part2_Enhanced_throughput.csv # Parallel connections vs Throughput
│   ├── Part1_throughput.png          # Generated scalability graph
│   ├── Part2_Cases_throughput.png    # Case 1 vs Case 2 comparison plot
│   └── Part2_Enhanced_throughput.png # Parallel scaling plot
│
├── docs/                             # Engineering and Audit Documentation
│   ├── ARCHITECTURE.md               # Detailed architectural deep-dive
│   ├── AUTOGRADER_CHECKLIST.md       # CLI and autograder compliance audit
│   ├── DEMO_PLAN.md                  # Interactive viva and evaluation script
│   ├── DESIGN_REVIEW.md              # Technical design justification
│   ├── Design_Document.md            # Comprehensive project design specification
│   ├── FINAL_AUDIT.md                # Quality gate verification matrix
│   ├── MODEL_DESIGN_COMPARISON.md    # Analysis against instructor model solution
│   ├── PROTOCOL.md                   # Full wire-protocol specification
│   ├── REQUIREMENTS_MATRIX.md        # Requirement traceability checklist
│   ├── RUN_GUIDE.md                  # Step-by-step terminal execution walkthrough
│   ├── SESSION_PROTOCOL.md           # Session management and recovery specification
│   ├── TESTING.md                    # Test execution and verification guide
│   └── VIVA.md                       # Comprehensive oral examination Q&A guide
│
├── FinalSubmission_Group_100_113/    # Clean submission package mirroring repo
├── FinalReport_Group_100_113.pdf     # Formal academic final report
├── report_execution_guide.md         # Multi-node physical machine testing guide
└── README.md                         # Primary project documentation (this file)
```

---

## 5. Compilation and Prerequisites

### 5.1 Environment Prerequisites
- **Compiler:** `gcc` (version 9.0 or higher supporting C99/C11 standards).
- **Libraries:** Standard C Library (`libc`), POSIX Threads (`-pthread`), Real-Time library (`-lrt`).
- **Build System:** GNU `make`.
- **Target OS:** POSIX-compliant Linux (Ubuntu 20.04/22.04/24.04 LTS or WSL2).
- **Optional Python Tools:** Python 3 with `pandas` and `matplotlib` (for generating benchmark graphs).

### 5.2 Building Modules

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

## 6. Execution and Operational Guide

### 6.1 Part I Execution

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

### 6.2 Part II Case 1 Execution (Fault Injection Without Sessions)

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
*Note: Under frequent disconnections, the client will report elevated `Redundant Bytes` and multiple reconnect cycles due to directory wipes.*

---

### 6.3 Part II Case 2 Execution (Stateful Checkpoint Resumption)

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

### 6.4 Part II Case 2 Enhanced Execution (Parallel Multi-Connection)

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

## 7. Verification and Automated Testing

### 7.1 Test Suite Organization

The repository provides a comprehensive automated test suite in [`tests/run_tests.sh`](tests/run_tests.sh). The harness executes 5 rigorous testing stages:
1. **Part 1 Functional Verification:** Compiles Part 1, launches server and client on loopback, transfers a multi-file synthetic dataset (text, 128 KB binary, 1 MB multi-chunk binary), verifies exact file matching via `diff`, and tests negative error handling with non-existent topic names.
2. **Part 2 Case 1 Verification:** Compiles Case 1, validates clean baseline transfer ($P=0.0$), injects faults ($P=0.02$), confirms that directory wipes occur on disconnect, and validates exact byte-for-byte final transfer integrity.
3. **Part 2 Case 2 Verification:** Compiles Case 2, tests baseline transfer ($P=0.0$), injects faults ($P=0.02$), verifies session recovery and byte-level resumption across disconnections, and verifies identical file checksums.
4. **Part 2 Case 2 Enhanced Verification:** Compiles Case 2 Enhanced, spawns multi-worker connections ($N=2$), validates round-robin workload partitioning, and verifies file integrity.
5. **Final Submission Package Verification:** Verifies that all code in `FinalSubmission_Group_100_113/` builds cleanly across all 4 directories without warnings or errors.

### 7.2 Running the Test Harness

```bash
cd tests
chmod +x run_tests.sh
./run_tests.sh
```

**Expected Output:**
```
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
Total Tests  : 14
Passed Tests : 14
Failed Tests : 0

🎉 ALL TESTS PASSED!
```

---

## 8. Performance Benchmarking and Analytics

### 8.1 Executing Automated Benchmarks

An automated parameter-sweep benchmarking suite is located in [`benchmark/run_benchmarks.sh`](benchmark/run_benchmarks.sh). It iterates over all 10 animal topics in the dataset, sweeping:
- **Part 1:** Client concurrency levels: 1, 2, 4, 8 clients.
- **Part 2 Cases 1 & 2:** Failure injection probabilities: $P \in \{0.00, 0.01, 0.02, 0.05, 0.10\}$.
- **Part 2 Enhanced:** Parallel connection counts: 1, 2, 4, 8 connections under $P=0.05$.

To execute the benchmark sweeps:
```bash
cd benchmark
chmod +x run_benchmarks.sh
./run_benchmarks.sh
```
*Outputs generated:*
- `Results/Part1_throughput.csv`
- `Results/Part2_throughput.csv`
- `Results/Part2_Enhanced_throughput.csv`

### 8.2 Generating Analytical Graphs

Ensure `python3`, `pandas`, and `matplotlib` are installed, then execute:
```bash
cd benchmark
python3 plot.py
```
*Generated plots in `Results/`:*
- `Part1_throughput.png`: Multi-client throughput scalability.
- `Part2_Cases_throughput.png`: Case 1 vs Case 2 throughput degradation curves under increasing failure probability.
- `Part2_Enhanced_throughput.png`: Throughput speedup achieved with parallel TCP worker streams.

### 8.3 Empirical Observations and Trade-offs

1. **Part I Concurrency:** Under low connection counts, multi-threaded performance scales linearly over single-threaded execution. At extreme concurrency, throughput plateaus due to local disk read/write bandwidth saturation.
2. **Case 1 Failure Cliff:** Because Case 1 wipes its output directory and restarts upon each drop, the probability of completing a large topic transfer without a disconnection decays exponentially ($P_{\text{success}} = (1 - P)^{\text{chunks}}$). At $P \ge 0.05$, throughput drops to near zero and redundant bytes skyrocket.
3. **Case 2 Resilience:** Session management eliminates redundant retransmissions ($0 \text{ bytes redundant}$ under reliable storage). As failure probability rises, completion time increases only by the small cost of TCP reconnects and resumption handshakes.
4. **Enhanced Parallelism:** Using 4 to 8 parallel TCP streams delivers up to a 4x throughput improvement over single-connection Case 2 transfers under identical loss rates, as disconnections on one worker do not stall remaining transfers.

---

## 9. Robustness, Autograder Compliance, and Quality Gates

| Evaluation Criteria | Implementation Verification | Status |
| :--- | :--- | :---: |
| **No Hardcoded IPs/Ports** | All endpoints and directories are passed via CLI arguments only. | **PASS** |
| **Exact CLI Compliance** | Parameter sequences match professor specifications across all parts. | **PASS** |
| **Standard Exit Codes** | Clean termination (`exit(0)`) on success; non-zero on critical errors. | **PASS** |
| **Socket Reuse** | `setsockopt(..., SO_REUSEADDR, ...)` configured across all server sockets. | **PASS** |
| **Signal Safety** | `MSG_NOSIGNAL` flag used in `send()` to prevent `SIGPIPE` termination. | **PASS** |
| **Thread Safety** | Server session table and client metric accumulators guarded by mutexes. | **PASS** |
| **Path Traversal Protection** | Requests containing `..` or `/` rejected with `TYPE_ERROR`. | **PASS** |
| **Partial I/O Handling** | `send_all()` and `recv_all()` prevent truncation from partial OS buffers. | **PASS** |
| **Byte-Order Standardization** | Big-Endian network byte order enforced across all 16, 32, and 64-bit fields. | **PASS** |

---

## 10. Complete Documentation Roadmap

In addition to this README, comprehensive technical documentation is provided across the repository:

| Document | Description |
| :--- | :--- |
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
| [`report_execution_guide.md`](report_execution_guide.md) | 3-node physical machine setup and evidence collection manual. |
| [`FinalReport_Group_100_113.pdf`](FinalReport_Group_100_113.pdf) | Formal project report with empirical charts and analysis. |
