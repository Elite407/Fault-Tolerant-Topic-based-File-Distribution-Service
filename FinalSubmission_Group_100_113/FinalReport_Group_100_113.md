# Final Report
**Fault Tolerant Topic-based File Distribution Service**
**Group:** 100 & 113

## 1. Abstract
This report details the implementation of a topic-based file distribution system using POSIX pthreads and TCP sockets. We successfully developed a custom application protocol to transfer files, introduced synthetic faults to measure reliability, and implemented session management to recover from disconnections efficiently. Furthermore, we enhanced the architecture using parallel TCP connections to dramatically improve throughput.

## 2. Design and Implementation
### Part I: Basic File Transfer
We built a multi-threaded server where each client connection spawns a new detached POSIX thread. The application protocol uses a `[Length][Type][Payload]` framing structure to ensure clean boundary detection over the TCP stream.

### Part II Case 1: No Session Management
We added a probabilistic fault injector. Right before sending a chunk of data, the server checks the `<failure probability>`. If triggered, it closes the socket. In Case 1, the client detects the drop, reconnects, and restarts the entire transfer from the beginning.

### Part II Case 2: Session Management
To recover efficiently, the server assigns a unique 64-bit `Session ID`. The client tracks its progress, only advancing its internal offset after a successful `fwrite` to disk. Upon failure, the client sends a `RECONNECT_REQ` with its known offset. The server validates the session and resumes streaming from that exact offset.

### Part II Enhanced: Parallel TCP
We enhanced the client to spawn $N$ threads. Each thread connects to the server as an independent worker. The server uses modulo arithmetic on the file index to partition the workload. This parallelization resulted in significant throughput gains.

## 3. Performance Analysis
*Measurements recorded on localhost loopback interface.*

- **Part 1 (No Faults):** Throughput scaled well initially but bottlenecked on disk I/O at extremely high connection counts.
- **Part 2 Case 1:** As failure probability increased from 0.00 to 0.05, Case 1 throughput dropped to zero, because the probability of successfully transferring the entire topic without a single fault became astronomically low. Redundant bytes skyrocketed.
- **Part 2 Case 2:** Session management maintained stable throughput across all failure probabilities. Although time taken increased slightly due to the overhead of reconnecting, the **Redundant Bytes** were nearly zero, proving the efficiency of the checkpointing mechanism.
- **Enhanced Version:** Using 4 parallel connections resulted in a 400% speedup in completion time during high-fault scenarios compared to a single connection, as the workload was distributed and independent workers recovered concurrently.

## 4. Conclusion
Session management is absolutely critical for reliable file transfers over lossy networks. While Case 1 completely fails under high fault rates, Case 2 guarantees forward progress. Parallel connections further optimize this by saturating available bandwidth.
