# Design Document
**Group Number:** 100_113
**Course:** IT305 LAB - Socket Programming Project

---

## 1. Server Data Structures for Topic Mapping
To map topics to directories, the server relies on the underlying filesystem rather than an in-memory hash map. 
The server uses a global `topics_root_dir` provided via command-line arguments. When a client requests a topic, the server constructs the directory path as `<topics_root_dir>/<topic>` and uses the `dirent.h` (POSIX directory entries) and `sys/stat.h` libraries to read the contents.
This design choice avoids redundant memory usage, seamlessly supports dynamic additions to topics in the filesystem, and prevents directory traversal attacks by validating the requested topic name (rejecting names with `..` or `/`).

---

## 2. Client-Server Message Formats
All communication follows a Type-Length-Value (TLV) framed protocol to handle TCP stream boundaries reliably. Every message begins with a 5-byte header defined by `msg_header_t`:
- **Length**: 4 bytes (uint32_t, Network Byte Order) representing the payload size.
- **Type**: 1 byte (uint8_t) defining the message type.

### Message Types across Parts I and II:
* **TYPE_GET_REQUEST (0x01):** Sent by client. Payload is the requested topic string.
* **TYPE_TOPIC_RESPONSE (0x02):** Sent by server. 1-byte status (Found/Not Found) + 4-byte total files count.
* **TYPE_FILE_META (0x03):** 4-byte file index + 8-byte file size + 2-byte filename length + filename string.
* **TYPE_FILE_CHUNK (0x04):** 4-byte file index + 8-byte chunk offset + chunk data (up to 8KB).
* **TYPE_FILE_EOF (0x05):** 4-byte file index indicating EOF for that specific file.
* **TYPE_ERROR (0x06):** String payload detailing the server-side error.
* **TYPE_TOPIC_EOF (0x07):** End of topic transmission. 0-byte payload.

### Case 2 & Enhanced Extensions:
* **TYPE_SESSION_CREATED (0x08):** Sent by server upon first connect. Payload is an 8-byte `session_id`.
* **TYPE_RECONNECT_REQ (0x09):** Sent by client after a crash. Payload: 8-byte `session_id` + 4-byte `resume_file_idx` + 8-byte `resume_offset` + topic string.
* **TYPE_RESUME_RESPONSE (0x0A):** 1-byte payload indicating `RESUME_ACCEPTED` (1) or `RESUME_REJECTED` (0).
* **TYPE_GET_REQUEST_WORKER (0x0B):** (Enhanced Case) Sent by worker threads in parallel client. Payload: 4-byte `worker_id` + 4-byte `total_workers` + topic string.

---

## 3. Fault Injection Mechanism
The fault injection mechanism is implemented directly within the server's file chunk transmission loop. A `<failure_probability>` argument dictates the likelihood of dropping the connection. 
During the `fread()` loop for every chunk, the server generates a random float `r` (seeded using `time(NULL) ^ pthread_self()` to ensure unique randomness across concurrent threads). If `r < failure_probability`, the server actively closes the file descriptor, tears down the socket, and terminates the thread prematurely.

---

## 4. Session Management Protocol
To provide fault tolerance (Case 2), the system implements a lightweight Session Management protocol. 
1. **Server-Side State:** The server maintains a linked list of `Session` structs (protected by a mutex). Each struct holds a randomly generated 64-bit `session_id` and the associated `topic`. 
2. **Client-Side State:** The client maintains the latest checkpoint in memory: `session_id`, `current_file_idx`, and `current_offset`. It updates `current_offset` after each successfully written `TYPE_FILE_CHUNK`.
3. **Recovery Sequence:** 
    - Upon disconnection, the client enters a retry loop, sleeping briefly between attempts.
    - On successful TCP connect, it sends a `TYPE_RECONNECT_REQ` with its checkpoint data.
    - The server validates the `session_id` against its linked list. If valid, it responds with `TYPE_RESUME_RESPONSE (RESUME_ACCEPTED)`.
    - The server then iterates over the directory, skipping files until it reaches `resume_file_idx`, seeks into the file to `resume_offset`, and resumes sending `TYPE_FILE_CHUNK`.

---

## 5. Performance Enhancements (Case 2 - Enhanced)
For the final enhanced version, we utilize **Parallel TCP Connections**.
Instead of a single TCP connection blocking on high-latency transfers, the client spawns multiple POSIX threads (workers). Each worker initiates its own TCP connection and sends a `TYPE_GET_REQUEST_WORKER` message containing its unique `worker_id` and the `total_workers` count. 
The server uses these parameters to split the file distribution. By assigning files to workers using a modulo operation (`if (file_index % total_workers == worker_id)`), the workload is distributed evenly across multiple parallel TCP streams, vastly improving throughput over high-bandwidth/high-latency networks.

---

## 6. Extra / Additional Properties
* **Security & Path Traversal Prevention:** The server explicitly inspects the requested topic for `../` and `/` characters, mitigating path traversal vulnerabilities.
* **Shared Protocol Abstraction:** The network serialization and deserialization routines (`send_all`, `recv_all`, byte swapping) are completely decoupled into `protocol.h` and `protocol.c`.
* **Thread-safe Metrics:** In the Enhanced parallel client, metrics (useful bytes vs total bytes, reconnect counts) are safely aggregated using mutexes across threads.

---

## 7. Overall Module / File Structure
The codebase follows a modular structure separated by cases. Inside each `Part/` folder:
- `protocol.h` & `protocol.c`: Defines the message headers, constants, and network I/O helper functions.
- `server.c`: Multi-threaded server implementation handling concurrent requests, directory traversal, and session logic.
- `client.c`: Client implementation that manages downloads, file creation, reconnect logic, and performance measurement.
- `Makefile`: Automates the compilation of both server and client binaries.

---

## 8. Design Thought Process & Alternatives Considered
* **Topic Mapping:** We initially considered using an in-memory hash table mapping topics to directory paths. However, this felt overly complex and prone to staleness if directories were modified while the server was running. Directly exploring `topics_root_dir` provided the most scalable and stateless approach.
* **Session Checkpointing:** For fault tolerance, we debated whether the server or the client should track the byte offsets. Storing byte offsets on the server would require large, frequently updated data structures, leading to heavy mutex contention and potential memory leaks if clients never reconnected. We opted for a **client-driven offset** model. The server merely tracks the active `session_id` for authorization, while the client dictates the exact byte offset to resume from. This keeps the server stateless regarding the transfer progress, making it highly resilient and scalable.
