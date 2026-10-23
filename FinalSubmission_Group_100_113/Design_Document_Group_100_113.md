# Design Document
**Group Number:** 100_113
**Course:** IT305 LAB - Socket Programming Project

---

## 1. Server Data Structures for Topic Mapping
To map topics to directories, the server relies on the underlying filesystem rather than an in-memory hash map. 
At startup, the server scans `topics_root_dir` (a command-line argument) and populates a global `topic_table[]` array (up to 128 entries). Each `TopicEntry` stores the topic name string and its full directory path. When a client requests a topic, the server performs a linear scan of this table for an exact match.
This design avoids redundant memory usage and prevents directory traversal attacks by validating the requested topic name (rejecting names with `..` or `/`).

---

## 2. Client-Server Message Formats
All communication follows a Type-Length-Value (TLV) framed protocol to handle TCP stream boundaries reliably. Every message begins with a **9-byte header** defined by `msg_header_t` (packed, no padding):

```
Byte 0:    uint8_t  type           — Message type identifier
Bytes 1–4: uint32_t transaction_id — Request/response correlation ID (network byte order)
Bytes 5–8: uint32_t length         — Payload length in bytes (network byte order)
```

> **Note:** An earlier version of this document described a 5-byte header. The actual implementation uses a 9-byte packed header as described above.

### Message Types across Parts I and II:
| Constant | Value | Direction | Description |
|---|---|---|---|
| `TYPE_GET_REQUEST`        | `0x01` | C→S | Request topic transfer. Payload: topic string |
| `TYPE_TOPIC_RESPONSE`     | `0x02` | S→C | 1-byte status + 4-byte file count (on FOUND) |
| `TYPE_FILE_META`          | `0x03` | S→C | 4-byte file index + 8-byte file size + 2-byte name length + filename |
| `TYPE_FILE_CHUNK`         | `0x04` | S→C | 4-byte file index + 8-byte offset + chunk data (≤8 KB) |
| `TYPE_FILE_EOF`           | `0x05` | S→C | 4-byte file index, signals one file complete |
| `TYPE_ERROR`              | `0x06` | S→C | String payload with error description |
| `TYPE_TOPIC_EOF`          | `0x07` | S→C | 0-byte payload, all files sent |
| `TYPE_MSG_START`          | `0x08` | C→S | Client ready to receive (sent after TOPIC_RESPONSE OK) |
| `TYPE_SESSION_CREATED`    | `0x09` | S→C | 8-byte `session_id` assigned for new connection (Case 2) |
| `TYPE_RECONNECT_REQ`      | `0x0A` | C→S | Resume: 8-byte `session_id` + 4-byte `file_idx` + 8-byte `offset` + topic string |
| `TYPE_RESUME_RESPONSE`    | `0x0B` | S→C | 1-byte: `RESUME_ACCEPTED` (1) or `RESUME_REJECTED` (0) |
| `TYPE_GET_REQUEST_WORKER` | `0x0C` | C→S | Parallel worker: 4-byte `worker_id` + 4-byte `total_workers` + topic string (Enhanced) |

### Status Codes:
- `STATUS_NOT_FOUND` = 0, `STATUS_FOUND` = 1 (in `TYPE_TOPIC_RESPONSE`)
- `RESUME_REJECTED` = 0, `RESUME_ACCEPTED` = 1 (in `TYPE_RESUME_RESPONSE`)

---

## 3. Fault Injection Mechanism
The fault injection mechanism is implemented directly within the server's file chunk transmission loop. A `<failure_probability>` argument dictates the likelihood of dropping the connection. 
During the `fread()` loop for every chunk, the server generates a random float `r` (seeded using `time(NULL) ^ pthread_self()` to ensure unique randomness across concurrent threads). If `r < failure_probability`, the server saves the current checkpoint (Case 2), closes the file descriptor, tears down the socket, and terminates the thread prematurely. Case 1 has no checkpoint; Case 2 saves `(file_index, offset)` to the session table before dropping.

---

## 4. Session Management Protocol
To provide fault tolerance (Case 2), the system implements a lightweight Session Management protocol.

1. **Server-Side State:** The server maintains a linked list of `Session` structs (protected by a `pthread_mutex_t`). Each struct holds a randomly generated 64-bit `session_id`, the associated `topic`, and the last confirmed checkpoint `(current_file_idx, current_offset)`.

2. **Checkpoint Semantics:** The server updates its checkpoint *after* each chunk is successfully sent (`offset += bytes_read; update_session_checkpoint(...)`). The server-side checkpoint therefore reflects the next byte to be sent, not the next byte the client has durably written. Clients should treat the offset as the point from which the server will resume sending.

3. **Server Checkpoint is Authoritative:** On reconnect, the server's recorded `(file_idx, offset)` is always used. The client's claimed values are ignored. This prevents a malicious or corrupt client from bypassing server-side progress tracking. A valid server checkpoint of `(0, 0)` means: "session was created but no data was confirmed sent" — this is a valid state and means restart from the beginning.

4. **Recovery Sequence:**
   - Upon disconnection, the client enters a retry loop with a 100 ms delay between attempts.
   - On reconnect, it sends `TYPE_RECONNECT_REQ` with its last known `session_id`, `file_idx`, and `offset`.
   - The server validates the `session_id`. If valid, it responds with `RESUME_ACCEPTED` and resumes from its own authoritative checkpoint.
   - The server skips files before `resume_file_idx` and seeks into the current file to `resume_offset`.

5. **Permanent vs. Transient Errors:** `STATUS_NOT_FOUND` is a **permanent application error** (the topic does not exist). The client terminates immediately without retry. Network disconnections (`recv_all` returning ≤0) are **transient errors** and trigger the reconnect loop.

---

## 5. Performance Enhancements (Case 2 - Enhanced)
For the final enhanced version, **Parallel TCP Connections** are used.
The client spawns `N` POSIX threads (workers). Each worker sends a `TYPE_GET_REQUEST_WORKER` message containing its unique `worker_id` (0 to N-1) and the `total_workers` count. 
The server assigns files to workers using a modulo operation: `if (file_index % total_workers == worker_id)`. This distributes the file list evenly across parallel TCP streams.

Each worker independently maintains its own session state, checkpoint, and reconnect loop. Metrics (total bytes, useful bytes, reconnect count) are aggregated into shared counters protected by a `pthread_mutex_t`. Worker success/failure is tracked per-thread; the overall exit code is 0 only if all workers completed successfully.

---

## 6. Extra / Additional Properties
* **Security & Path Traversal Prevention:** The server explicitly inspects the requested topic for `../` and `/` characters, rejecting invalid names before any filesystem access.
* **Shared Protocol Abstraction:** Network serialization (`send_all`, `recv_all`, `htonll`/`ntohll`) is decoupled into `protocol.h` and `protocol.c`.
* **Thread-safe Metrics:** In the Enhanced parallel client, metrics are safely aggregated using a dedicated `pthread_mutex_t`.
* **SIGPIPE Prevention:** `send_all()` uses `MSG_NOSIGNAL` flag so that writing to a disconnected peer returns `EPIPE` rather than raising SIGPIPE and killing the process.
* **File-Open Error Reporting:** If a file that was listed by `stat()` becomes unreadable by the time `fopen()` is called, the server sends a `TYPE_ERROR` message and closes the connection, rather than silently emitting a `TYPE_FILE_EOF` for empty data.
* **Payload Validation:** Before accessing fixed-offset fields in `TYPE_FILE_META` and `TYPE_FILE_CHUNK` payloads, the receiver validates that `payload_len` is sufficient and that the filename length does not exceed the payload or the destination buffer.

---

## 7. Overall Module / File Structure
Inside each `Part*/` folder:
- `protocol.h` & `protocol.c`: Message headers, constants, `send_all`, `recv_all`, byte-swap helpers.
- `server.c`: Multi-threaded server handling concurrent clients, directory traversal, session logic.
- `client.c`: Client managing downloads, file creation, reconnect logic, and performance measurement.
- `Makefile`: Automates compilation with `-Wall -Wextra -Wpedantic -O2 -pthread`.

---

## 8. Design Thought Process & Alternatives Considered
* **Topic Mapping:** A startup scan to an array is simpler and more predictable than a runtime hash table. Directories that disappear after startup do not affect the table.
* **Session Checkpointing:** The server tracks `(file_idx, offset)` and is always authoritative. This prevents stale or malicious client offsets from bypassing recovery. The tradeoff is that the server's checkpoint reflects sent-but-not-acknowledged data; a crash between the chunk send and the checkpoint update could cause re-sending of already-received data on resume (which is safe because the client uses `fopen("ab")` and `fseek` before writing). There is no explicit client-to-server acknowledgement per chunk — that would require a full stop-and-wait protocol not specified in the assignment.
* **No fsync:** `fwrite` is used without `fsync`. This is appropriate for the assignment's fault model (network disconnection, not disk failure). Adding `fsync` per chunk would reduce throughput by orders of magnitude and is not required.
