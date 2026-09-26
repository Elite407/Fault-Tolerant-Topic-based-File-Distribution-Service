# Design Document: Fault Tolerant Topic-based File Distribution Service

## 1. Cover Information / Project Title
**Project Title:** Fault Tolerant Topic-based File Distribution Service
**Group Numbers:** 100 and 113
**Course:** IT305 LAB : Socket Programming Project

## 2. Problem Statement
The objective is to implement a topic-based file distribution system over TCP. The system must support single-threaded and multi-threaded execution, handle partial transfers correctly, inject artificial faults (connection drops) to test resilience, and support recovery. Recovery is implemented both naively (restarting from scratch) and intelligently (resuming from a checkpoint using session management). Performance must be measured and enhanced.

## 3. Objectives
- Master TCP socket programming (handling byte streams, framing).
- Implement efficient multi-threading using POSIX pthreads.
- Build resilient fault-tolerant distributed systems.
- Understand the performance impacts of multithreading, session state, and parallel connections.

## 4. Requirements
*MANDATORY FROM PROFESSOR*
- Server maps topics to directories containing files.
- Client requests a topic and receives all files.
- Part I: Implement single-threaded and multi-threaded servers, measure and compare throughput.
- Part II Case 1: Inject faults; client must reconnect and restart transfer from the beginning. No session state.
- Part II Case 2: Maintain session state. After a fault, client reconnects and transfer resumes from the last checkpoint.
- Part II Enhanced: Improve performance (we chose Parallel TCP Connections).
- Automated CLI interface must be strictly preserved (`./server <port> <dir>`, `./client <ip> <port> <topic> <out>`).
- No hardcoded paths, IPs, or ports.

## 5. System Architecture
The system employs a Client-Server architecture over standard TCP. A custom application-layer protocol is defined to frame messages. The server handles file reading and network transmission, while the client handles network reception and disk writing.

## 6. Server Architecture
The multi-threaded server listens on a main socket. Upon `accept()`, a new POSIX thread is spawned to handle the client connection. The thread parses requests, reads file metadata from the local filesystem, constructs protocol messages, and sends them via the socket.

## 7. Client Architecture
The client connects to the server and issues a `GET_REQUEST`. It loops receiving messages: `FILE_META` (to open a new local file) and `FILE_CHUNK` (to write data to the file). It maintains local state for checkpointing.

## 8. Topic-to-directory Data Structure
*MANDATORY FROM PROFESSOR*
The topics are animal names (e.g., `Cane`). The server maps this to `<topics_root_dir>/Cane`.
*OUR DESIGN DECISION*
The server does not need to preload all files into memory. It dynamically constructs the directory path, uses `opendir()`/`readdir()`, and constructs a list of files dynamically when a topic is requested. This is memory efficient.

## 9. Application-layer Protocol
Since TCP is a stream, we define a binary protocol:
`| Message Length (4 bytes) | Type (1 byte) | Payload (Variable) |`
Types include: `GET_REQUEST`, `TOPIC_RESPONSE`, `FILE_META`, `FILE_CHUNK`, `FILE_EOF`, `TOPIC_EOF`.

## 10. Part I Design
Basic file transfer without fault injection. The server loops through all files in the topic directory, sending metadata, followed by chunks of the file until EOF.

## 11. Single-threaded Server Design
The main loop blocks on `accept()`. Once a client connects, the server processes the entire topic transfer before calling `accept()` again. Other clients must wait in the listen backlog.

## 12. Multi-threaded Server Design
The main loop accepts a connection, dynamically allocates memory for the client socket descriptor, and spawns a thread (`pthread_create`). The thread is detached so resources are reclaimed automatically. The main loop immediately returns to `accept()`.

## 13. Part II Architecture
Introduces random faults. The protocol is robust enough to handle unexpected TCP disconnections.

## 14. Fault Injection Mechanism
*MANDATORY FROM PROFESSOR*
The server takes a `<failure probability>` argument.
*OUR DESIGN DECISION*
The failure decision point occurs right before sending a `FILE_CHUNK`. We generate a random float `r` between 0 and 1. If `r < failure_probability`, the server intentionally calls `close()` on the client socket, logs the fault, and terminates the thread.

## 15. Case 1 Design
No session management. If the client receives a 0-byte read or socket error, it closes its socket, sleeps briefly, reconnects, and sends a new `GET_REQUEST`.

## 16. Case 2 Design
Session Management. The server sends a unique `SESSION_CREATED` (containing a 64-bit random Session ID). The client stores this in memory.

## 17. Session Management
*OUR DESIGN DECISION*
The server tracks active sessions in a linked list (protected by a mutex). A session object stores the Session ID and the topic name.
The client tracks how many bytes it has successfully written to disk.

## 18. Checkpoint Mechanism
The checkpoint is conceptually `(File Index, Byte Offset)`.
*OUR DESIGN DECISION*
We use the client's local disk as the definitive checkpoint. The client updates its offset variable only after `fwrite()` succeeds.

## 19. Resume Protocol
Upon failure, the client connects and sends `RECONNECT_REQUEST (Session ID, Topic, File Index, Offset)`.
The server looks up the Session ID, verifies the topic matches, and skips files/bytes to reach the exact offset, then resumes sending `FILE_CHUNK`s.

## 20. Case 2 Enhanced Design
*OUR DESIGN DECISION*
Parallel TCP Connections. The client spawns $N$ threads, each creating a separate TCP socket to the server. The client requests a subset of the files on each connection (e.g. Worker 0 handles even files, Worker 1 handles odd files). The server concurrently streams these files.

## 21. Performance Enhancements
Parallelism effectively utilizes available bandwidth, especially if single connections are limited by TCP window scaling on high-latency networks.

## 22. Performance Measurement Methodology
Time is measured on the Client side using `clock_gettime(CLOCK_MONOTONIC)`. Time starts right before `connect()` and stops when `TOPIC_EOF` is received.

## 23. Metrics
- **Completion Time:** Total elapsed time in seconds.
- **Total Bytes Transmitted:** Counted at the socket level.
- **Useful Bytes:** Exact size of the files requested.
- **Throughput:** `Useful Bytes / Completion Time` (in bytes/sec).
- **Redundant Bytes:** `Total Bytes - Useful Bytes`.

## 24. Error Handling
All `send()`/`recv()` calls are wrapped in loops to handle partial transfers and `EINTR`. Return values of `<= 0` cleanly terminate the thread/connection.

## 25. Thread Synchronization
In Case 2, the global Session Map on the server is protected by a `pthread_mutex_t`. File writes on the client in the Enhanced Case do not require mutexes because each worker thread writes to completely separate files.

## 26. File-transfer Mechanism
Files are read using `fread()` in 8KB chunks. Chunks are sent via the custom protocol. This avoids reading gigabytes of data into RAM at once.

## 27. Module/File Structure
- `protocol.h`, `protocol.c`: Message framing, network byte order handling.
- `server.c`: Main server loop, thread management, fault injection.
- `client.c`: Client logic, writing files, tracking checkpoints, performance measurement.

## 28. Key Functions
- `send_message()` / `recv_message()`
- `handle_client()` (Server thread entry)
- `resume_transfer()` (Case 2 server logic)
- `process_topic()` (Server directory reading)

## 29. State Machines
The client operates in a state machine: `CONNECTING -> REQUESTING -> RECEIVING -> RECONNECTING -> FINISHED`.

## 30. Sequence Diagrams
(See ARCHITECTURE.md and SESSION_PROTOCOL.md for Mermaid diagrams).

## 31. Alternatives Considered
*Alternative:* Checkpoint on Server. The server could track what it sent.
*Why Rejected:* If the server sends a chunk but the connection drops before the client receives/writes it, the server will think the chunk is safely delivered, causing data corruption. Client-side truth is safer.

## 32. Design Decisions
- Binary protocol over text: More efficient for file transfers, avoids parsing issues with binary files (images).
- Pthreads over `select`/`epoll`: Required by PDF ("multi-threaded server"), simpler to reason about blocking I/O with fault injection.

## 33. Trade-offs
Parallel TCP connections increase throughput but use more file descriptors and server threads.

## 34. Testing Strategy
- Validation with valid/invalid topics.
- Concurrency testing with scripts spawning many clients.
- Fault testing by injecting high failure probabilities.
- Comparing MD5 checksums of original and received directories to ensure zero data corruption.

## 35. Extra Features
- Detailed colored console logging (`logger.h`).
- Structured metric collection for easy CSV generation.

## 36. Extensibility
The protocol includes message types, making it trivial to add `GET_FILE_INFO` or `LIST_TOPICS` in the future.

## 37. Expected Failure Scenarios
- Server crash (client will retry and eventually time out).
- Client disk full (handled by checking `fwrite` returns, failing cleanly).

## 38. Limitations
- Security: No encryption or authentication. Path traversal is mitigated by stripping `/` and `..` from topic names.
- Memory: Large numbers of parallel connections in the Enhanced version could exhaust server threads (C10k problem).

## 39. Future Improvements
- Move from Thread-Per-Client to a Thread Pool or `epoll` + Thread Pool for better scalability.
- Implement UDP-based reliable transfer for even higher throughput over lossy networks.
