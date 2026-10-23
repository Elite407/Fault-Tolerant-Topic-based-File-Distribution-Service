# Implementation Fixes and Viva Preparation
**Group 100_113 — IT305 LAB Socket Programming Project**  
**Last updated:** October 2026

---

## 1. Critical Bugs Fixed

### Bug A — Infinite retry loop for nonexistent topics
**Files:** `Part2_Case1/client.c`, `Part2_Case2/client.c`, `Part2_Case2_Enhanced/client.c`

**Root cause:** `STATUS_NOT_FOUND` set `error_occurred = 1` and broke out of the inner receive loop. The outer `while (!success && error_occurred)` block then treated it like a transient network failure: it cleaned up the partial directory and reconnected — indefinitely.

**Fix:** Added a `topic_not_found` flag (distinct from `error_occurred`). When `STATUS_NOT_FOUND` is received, `topic_not_found = 1` is set. The outer loop condition changes to `while (!success && !topic_not_found)`, and the post-loop cleanup block checks `topic_not_found` first, breaks immediately without calling `clean_topic_dir()`, and prints a clear error.

**Corrected behavior:** Requesting a nonexistent topic prints `[ERROR] Topic '...' not found on server. Exiting.` and exits in under one second with 0 reconnect attempts.

---

### Bug B/H — Valid (0, 0) server checkpoint treated as absent
**Files:** `Part2_Case2/server.c`, `Part2_Case2_Enhanced/server.c`

**Root cause:** Resume logic contained:
```c
resume_file_idx = (srv_file_idx > 0 || srv_offset > 0) ? srv_file_idx : client_file_idx;
resume_offset   = (srv_file_idx > 0 || srv_offset > 0) ? srv_offset   : client_offset;
```
When a session was created but disconnected before any chunk was sent (or at the exact start of a file), the server's checkpoint is legitimately `(0, 0)`. The condition `(srv_file_idx > 0 || srv_offset > 0)` evaluates `false`, so the server fell back to the client's *claimed* values — which could be anything the client sends.

**Fix:** The server checkpoint is always authoritative. Replaced with:
```c
resume_file_idx = srv_file_idx;
resume_offset   = srv_offset;
```
A `(0, 0)` checkpoint means "resend everything from the start," which is correct.

**Corrected behavior:** Resuming from `(0, 0)` causes the server to restart the full transfer. Resuming from `(3, 81920)` causes the server to skip files 0–2 and seek 80 KB into file 3.

---

### Bug C — Unsafe pthread_create in Enhanced client
**Files:** `Part2_Case2_Enhanced/client.c`

**Root cause:**  
1. `pthread_create()` return value was ignored; on failure the code still called `pthread_join()` on an uninitialised `pthread_t`, causing undefined behaviour.  
2. No per-worker success tracking; `printf("All files received successfully.\n")` was absent; even if all workers crashed, the client exited 0.  
3. No overall success/failure exit code.

**Fix:**
- Added `int success` field to `worker_args_t`, set to 1 by each worker on successful `TYPE_TOPIC_EOF` receipt.
- Added `int *thread_created` array; `pthread_create()` result stored per-worker.
- Join loop only joins successfully-created threads (`thread_created[i] == 1`).
- Post-join loop counts `workers_succeeded` and `workers_failed`, prints per-failed-worker error.
- Exit code is 1 if any worker failed.

**Corrected behavior:** `[RESULT] All 4 workers completed successfully.` only prints when all succeeded. Failed workers print `[ERROR] Worker N did not complete successfully.` and the process exits 1.

---

### Bug D — Unvalidated payload access in FILE_META and FILE_CHUNK handlers
**Files:** All client implementations (Part1, Case1, Case2, Enhanced)

**Root cause:** `TYPE_FILE_META` handler read `payload[12]` and `payload[14]` (fixed offsets for `nlen` and filename) without first verifying `payload_len >= 14`. If the server sent a malformed or truncated message, the client would read beyond the allocated buffer. Similarly, `payload_len - 12` in `TYPE_FILE_CHUNK` could underflow if `payload_len < 12`.

**Fix:** Added before every fixed-offset access:
```c
if (payload_len < 14) { /* error, break */ }
if ((uint32_t)(14 + nlen) > payload_len || nlen >= 1024) { /* error, break */ }
if (payload_len < 12) { /* error, break */ }  // FILE_CHUNK
```
On validation failure: free payload, set `error_occurred = 1`, break inner loop.

---

### Bug E — fwrite() return value unchecked
**Files:** `Part1/client.c`, `Part2_Case1/client.c` (Case2/Enhanced already checked but had incomplete error path)

**Root cause:** `fwrite()` return value was discarded. A partial write (disk full, permission error) would silently advance progress tracking without writing any data, producing corrupt output files.

**Fix:** All `fwrite()` calls now compare the return value against `data_len`. On mismatch, log the error and set `error_occurred = 1`. The checkpoint `current_offset` is not advanced on write failure. Case2/Enhanced already tracked `w == data_len` but now also break out of the receive loop on failure.

---

### Bug F — SIGPIPE could terminate the server process
**Files:** `protocol.c` in all four directories

**Root cause:** `send_all()` called `send(sockfd, ..., 0)`. On Linux, writing to a socket whose peer has closed raises `SIGPIPE`, which by default terminates the process — silently killing the server thread (or the whole process if SIGPIPE disposition is SIG_DFL).

**Fix:** Changed the flag from `0` to `MSG_NOSIGNAL` in all four `protocol.c` files:
```c
ssize_t n = send(sockfd, p + total, len - total, MSG_NOSIGNAL);
```
`EPIPE` is now returned as an error code and handled by the existing `-1` check. A disconnected client causes a clean thread exit, not a process crash.

---

### Bug G — fopen failure silently emits FILE_EOF
**Files:** `Part1/server.c`, `Part2_Case1/server.c`, `Part2_Case2/server.c`, `Part2_Case2_Enhanced/server.c`

**Root cause:** `fopen(filepath, "rb")` was inside `if (f) { ... }`. If `fopen` failed (file removed after `stat()`), the server would skip all chunk sending but then fall through to send `TYPE_FILE_EOF`. The client would interpret this as a successful transfer of an empty file.

**Fix:** Changed to `if (!f) { send_error(...); goto cleanup; }` pattern. The server now explicitly signals an error via `TYPE_ERROR` and aborts the transfer, preventing the client from recording a false successful completion.

---

### Bug I — Design Document had wrong header size and message type values
**File:** `Design_Document_Group_100_113.md`

**Root cause:** Document stated "5-byte header (type + length)" and used wrong numeric values for Case 2 extension messages (e.g., `SESSION_CREATED = 0x08`, conflicting with `TYPE_MSG_START`).

**Fix:** Document updated to match the actual implementation:
- Header: **9 bytes** (`type:1 + transaction_id:4 + length:4`)
- `TYPE_MSG_START = 0x08`, `TYPE_SESSION_CREATED = 0x09`, `TYPE_RECONNECT_REQ = 0x0A`, `TYPE_RESUME_RESPONSE = 0x0B`, `TYPE_GET_REQUEST_WORKER = 0x0C`

---

## 2. Socket Programming Concepts

### TCP as a Stream
TCP provides a reliable, ordered byte stream — not a message stream. There is **no guarantee** that a single `send()` will be received as a single `recv()`. A 9-byte header could be split across two `recv()` calls. `recv_all()` and `send_all()` loop until all bytes are transferred or an error occurs.

### Partial Sends/Receives
`send()` may return fewer bytes than requested (not an error — just partial write). The `send_all()` loop handles this:
```c
while (total < len) {
    n = send(sockfd, p + total, len - total, MSG_NOSIGNAL);
    if (n == -1 && errno == EINTR) continue;  // interrupted by signal, retry
    if (n <= 0) return n;                      // error or closed
    total += n;
}
```

### Message Framing (TLV)
To reliably delimit messages on a byte stream, we use Type-Length-Value framing: every message starts with a fixed-size header whose `length` field tells the receiver exactly how many payload bytes to read next. This avoids the need for delimiters and handles arbitrary payload sizes.

### Disconnection Detection
On Linux, a peer disconnect is detected when `recv()` returns 0 (clean close) or -1 with `errno == ECONNRESET` (abrupt close). Our `recv_all()` returns `0` or `-1`, which clients check via `<= 0`.

### SIGPIPE
Writing to a socket whose read-end is closed raises `SIGPIPE`. If not handled, this terminates the process. Solution: use `MSG_NOSIGNAL` flag on `send()`, which converts SIGPIPE to an `EPIPE` errno.

---

## 3. Protocol Explanation

### Header (9 bytes, packed, big-endian fields):
```
[type: 1 byte][transaction_id: 4 bytes, NBO][length: 4 bytes, NBO]
```

### Full Transfer Sequence (fresh connection):
```
Client → Server: TYPE_GET_REQUEST   (topic name in payload)
Server → Client: TYPE_TOPIC_RESPONSE (STATUS_FOUND + file count)
Server → Client: TYPE_SESSION_CREATED (session_id, Case 2 only)
Client → Server: TYPE_MSG_START     (zero payload — "ready")
Server → Client: TYPE_FILE_META     (file index + size + name)
Server → Client: TYPE_FILE_CHUNK    (file index + offset + data) × N
Server → Client: TYPE_FILE_EOF      (file index)
... (repeat FILE_META/CHUNK/EOF for each file) ...
Server → Client: TYPE_TOPIC_EOF     (zero payload — "done")
```

### Resume Sequence (Case 2 reconnect):
```
Client → Server: TYPE_RECONNECT_REQ (session_id + file_idx + offset + topic)
Server → Client: TYPE_RESUME_RESPONSE (RESUME_ACCEPTED)
Server → Client: TYPE_FILE_META/CHUNK/EOF (from server's checkpoint)
...
Server → Client: TYPE_TOPIC_EOF
```

### Validation Rules:
- `TYPE_TOPIC_RESPONSE`: `payload_len >= 1`
- `TYPE_FILE_META`: `payload_len >= 14`; `nlen` field checked: `14 + nlen <= payload_len` and `nlen < 1024`
- `TYPE_FILE_CHUNK`: `payload_len >= 12`; `data_len = payload_len - 12` (guaranteed non-negative)
- Any unexpected message type or validation failure → `error_occurred = 1`, break inner loop

---

## 4. Fault Tolerance

### Case 1 — Retry from scratch
No checkpoint. On disconnection, client wipes the partial topic directory and restarts. Guaranteed to eventually succeed if the server becomes available. Redundant bytes grow linearly with the number of retries × total data size.

### Case 2 — Checkpoint and resume
Server-side checkpoint: `(current_file_idx, current_offset)` updated after each chunk is sent.  
Client-side: `current_offset` updated after successful `fwrite`. The client's offset is sent in `RECONNECT_REQ` but **not used** — the server's record is authoritative.

**At start of new session:** Checkpoint is `(0, 0)` — valid, means "start from beginning."  
**At mid-file:** `(file_idx=3, offset=81920)` — server seeks 80 KB into file 3, sends the rest.  
**On RESUME_REJECTED:** Client resets all state and restarts from scratch (as if fresh connection).  
**Invalid state:** If `resume_file_idx >= total_files` or `resume_offset > file_size`, the `fseek()` will position past EOF and `fread()` returns 0 immediately — the server sends a `TYPE_FILE_EOF` with no chunks, which is safe.

### Checkpoint Durability
Checkpoints are in-memory only (the server's linked list). If the **server** process crashes and restarts, sessions are lost and the client will receive `RESUME_REJECTED`, triggering a full restart. This limitation is explicitly documented here and is within the assignment's scope.

---

## 5. File Integrity

### Write verification
All `fwrite()` calls compare the returned byte count to `data_len`. A mismatch (disk full, permission error) triggers `error_occurred = 1`, which closes the connection and triggers a reconnect (Case 2 can resume from the last server checkpoint).

### Progress tracking
- Case 1: `useful_bytes_received` incremented only on successful `fwrite`.
- Case 2/Enhanced: `current_offset = offset + data_len` advanced only on `fwrite` success.

### Checkpoint persistence limitations
Checkpoints live in server RAM. They survive client disconnection but not server restart. The assignment does not require persistent (disk-backed) checkpoints.

### Partial write and resume safety
On resume, the server seeks to `resume_offset` and re-sends from there. The client opens the file in append mode (`"ab"`) and uses `fseek` to position before writing. Because the server's checkpoint always points to the last acknowledged sent position, re-sent data lands at the correct offset, preventing gaps and duplicate data.

---

## 6. Multithreading (Enhanced Client)

### Worker lifecycle
1. Main thread creates `worker_args_t` structures, calls `pthread_create()` for each.
2. Each worker runs its own connection + retry loop independently.
3. On `TYPE_TOPIC_EOF`, worker sets `args->success = 1`.
4. Worker updates shared metrics under `metrics_mutex` before returning.
5. Main thread joins all successfully-created workers, aggregates `workers_succeeded` and `workers_failed`.

### pthread_create failure
If `pthread_create()` fails (resource exhaustion), `thread_created[i] = 0`. The join loop skips those indices. They are counted in `workers_failed`. Final exit code is 1.

### Shared state protection
- `total_bytes_received`, `useful_bytes_received`, `reconnects`: updated under `metrics_mutex`.
- `args[i].success`: written by worker `i` only; read by main thread only after join — no race.
- Server's `sessions_head` linked list: all access under `sessions_mutex`.

### Server-side threading
The server spawns one detached thread per client connection. Each thread handles exactly one transfer session from start to finish. `sessions_mutex` protects the global session list. `rand_r(&seed)` (thread-local seed) provides thread-safe fault injection randomness.

---

## 7. Error-Handling Decisions

### Permanent vs. Transient Errors
| Error | Classification | Reason |
|---|---|---|
| `STATUS_NOT_FOUND` | **Permanent** | Topic does not exist on server; retrying will always get the same answer |
| `recv_all()` returns ≤0 | **Transient** | Network interruption; server may still be running |
| `fwrite()` partial write | **Transient** | Disk issue; reconnect and resume from last checkpoint |
| `RESUME_REJECTED` | **Transient → Restart** | Session lost; restart from scratch (no retry of reconnect) |
| `TYPE_ERROR` message from server | **Transient** | Server-side error (e.g., file read error); retry may succeed |

### Why not retry `STATUS_NOT_FOUND`?
The topic table is built at server startup and never changes at runtime. If the topic is not in the table at time T, it will not be there at time T+1. Retrying wastes time and resources, and for Case 1, each retry would uselessly clean and recreate the output directory.

---

## 8. Demonstration Commands

All binaries are in their respective `Part*/` subdirectories under `FinalSubmission_Group_100_113/`.

### Part 1
```bash
# In FinalSubmission_Group_100_113/Part1/
make
./server <port> <topics_root_dir> [single|multi]
./client <server_ip> <port> <topic> <output_dir>

# Example:
./server 8080 ./dataset/ multi &
./client 127.0.0.1 8080 sports ./received/
```

### Part 2 Case 1 (Reconnect, restart from scratch)
```bash
# In FinalSubmission_Group_100_113/Part2_Case1/
make
./server <port> <topics_root_dir> <failure_probability>
./client <server_ip> <port> <topic> <output_dir>

# Example (10% fault rate):
./server 8081 ./dataset/ 0.1 &
./client 127.0.0.1 8081 sports ./received/
```

### Part 2 Case 2 (Session + checkpoint resume)
```bash
# In FinalSubmission_Group_100_113/Part2_Case2/
make
./server <port> <topics_root_dir> <failure_probability>
./client <server_ip> <port> <topic> <output_dir>

# Example (30% fault rate):
./server 8082 ./dataset/ 0.3 &
./client 127.0.0.1 8082 sports ./received/
```

### Part 2 Case 2 Enhanced (Parallel workers)
```bash
# In FinalSubmission_Group_100_113/Part2_Case2_Enhanced/
make
./server <port> <topics_root_dir> <failure_probability>
./client <server_ip> <port> <topic> <output_dir> [num_connections]

# Example (4 workers, 20% fault rate):
./server 8083 ./dataset/ 0.2 &
./client 127.0.0.1 8083 sports ./received/ 4
```

---

## 9. Manual Test Checklist

> **Note:** Results below are NOT fabricated. These are the expected behaviors to verify manually. Mark each as PASS/FAIL during your actual execution.

| # | Test | Expected Behavior |
|---|---|---|
| 1 | Part1 valid topic | All files received, MD5 matches originals, "All files received successfully." |
| 2 | Part1 invalid topic | "Topic '...' not found on server." printed; process exits immediately |
| 3 | Part1 empty file | Empty file created in output directory |
| 4 | Case1 valid topic, no faults | Same as Test 1; Reconnects = 0 |
| 5 | Case1 invalid topic | "Exiting." printed; Reconnects = 0; terminates in <1s |
| 6 | Case1 with fault probability | Multiple reconnects visible; directory wiped between attempts; eventually completes |
| 7 | Case2 valid topic, no faults | All files correct; Session created message shown; Reconnects = 0 |
| 8 | Case2 invalid topic | Terminates immediately; Reconnects = 0 |
| 9 | Case2 with fault injection | "[CASE 2] Reconnecting. Session: ..., File: N, Offset: M" messages appear; final files correct |
| 10 | Case2 checkpoint at (0,0) | Kill server mid-first-chunk; reconnect; "[SESSION] Resuming … at file=0 offset=0"; full retransfer |
| 11 | Case2 RESUME_REJECTED | Use session_id from killed server process; client resets and restarts fresh |
| 12 | Enhanced valid topic | "Workers Succeeded: N"; "[RESULT] All N workers completed successfully."; exit code 0 |
| 13 | Enhanced invalid topic | Worker(s) print not-found error; exit code 1 |
| 14 | Server disconnect mid-transfer | Client detects `recv_all ≤ 0`, enters retry loop |
| 15 | SIGPIPE handling | Kill client mid-send; server thread exits cleanly without crashing |

---

## 10. Viva Questions and Answers

**Q1: What is the purpose of `send_all()` and `recv_all()`?**  
A: TCP is a byte stream, not a message stream. A single `send()` may send fewer bytes than requested (partial write), and a single `recv()` may receive fewer bytes than expected (partial read). `send_all()` and `recv_all()` loop until the requested number of bytes have been transferred or an error occurs, ensuring complete messages are sent/received atomically from the application's perspective.

**Q2: What is the protocol header format and why is it 9 bytes?**  
A: The header is a packed struct: 1 byte `type` + 4 bytes `transaction_id` (NBO) + 4 bytes `length` (NBO) = 9 bytes. `#pragma pack(push, 1)` removes structure padding so the in-memory layout exactly matches the on-wire format. The `length` field enables TLV framing — the receiver reads a fixed-size header first, then reads exactly `length` more bytes for the payload.

**Q3: What does `MSG_NOSIGNAL` do and why do we use it?**  
A: On Linux, writing to a socket whose peer has closed raises `SIGPIPE`. By default, `SIGPIPE` terminates the process. `MSG_NOSIGNAL` suppresses this signal, instead returning `-1` with `errno = EPIPE`. We use it so that a disconnected client causes a clean thread/function exit via the existing error-handling path, not an unexpected process termination.

**Q4: How does Case 2 resume a transfer after disconnection?**  
A: The server maintains an in-memory session table. After each chunk is sent, it records `(session_id, file_index, offset)`. On reconnect, the client sends `TYPE_RECONNECT_REQ` with its `session_id`. The server looks up the session, responds with `RESUME_ACCEPTED`, then skips files before `resume_file_idx` and seeks into the current file to `resume_offset` before resuming chunk transmission.

**Q5: Why is the server's checkpoint authoritative rather than the client's?**  
A: The server's record reflects what was actually transmitted (from the server's perspective). The client could provide incorrect or malicious values. Using the server's record prevents a client from claiming it has received more data than it has (skipping data) or less (causing redundant retransmission beyond what the server knows about). A fresh session starts at `(0, 0)`, which the server stores from the moment of session creation.

**Q6: Why does `STATUS_NOT_FOUND` cause the client to exit immediately instead of retrying?**  
A: The topic table is populated once at server startup and never changes at runtime. If a topic does not exist at time T, it will still not exist at time T+1. Retrying would waste time and resources with a guaranteed failure. Additionally, in Case 1, each retry cleans the output directory — retrying indefinitely would loop cleanup forever.

**Q7: What happens if `fwrite()` writes fewer bytes than expected?**  
A: We compare the return value of `fwrite()` to `data_len`. If they differ, we log an error (`[ERROR] fwrite failed: wrote Z of N bytes`), set `error_occurred = 1`, and break out of the receive loop. The connection is closed. In Case 2, the server's checkpoint was updated before the failed write, so on reconnect the server will resend the data from its last checkpoint, giving the client another chance to write it.

**Q8: How do multiple Enhanced workers avoid writing conflicting data to the same file?**  
A: Files are distributed across workers using `file_index % total_workers == worker_id`. Each worker receives a disjoint subset of files. Within a single file, chunks arrive in offset order (the server sends them sequentially). Multiple workers never write to the same file simultaneously, so no file locking is needed.

**Q9: What happens if `pthread_create()` fails in the Enhanced client?**  
A: We check the return value. If non-zero, we log `[ERROR] pthread_create for worker N failed: ...`, set `thread_created[N] = 0`, and continue trying to create remaining workers. In the join loop, we skip indices where `thread_created[i] == 0`. The failed thread is counted in `workers_failed`. The process exits 1 if `workers_failed > 0`.

**Q10: How does the server handle an empty file in the topic?**  
A: `stat()` reports `st_size = 0`. The server sends `TYPE_FILE_META` (with size=0), then immediately `fread()` returns 0 on the first call (nothing to read), so no `TYPE_FILE_CHUNK` messages are sent. The server sends `TYPE_FILE_EOF`. The client calls `fopen(filepath, "wb")` or `"ab"` to create the file, then `fclose()` on `FILE_EOF`, resulting in a zero-byte file — correct behavior.

**Q11: How does the client detect that the server has disconnected?**  
A: `recv_all()` calls `recv()` in a loop. When the server closes the socket, `recv()` returns 0 (graceful shutdown) or -1 with `errno = ECONNRESET` (abrupt close). `recv_all()` propagates the non-positive return to the caller. The client checks `if (recv_all(...) <= 0)`, sets `error_occurred = 1`, and breaks out of the receive loop to trigger reconnect.

**Q12: What is network byte order, and where does this project convert between host and network byte order?**  
A: Network byte order is big-endian (most significant byte first). x86 Linux systems use little-endian host byte order. All multi-byte integer fields in protocol messages (`transaction_id`, `length`, file index, file size, offset) are converted: `htonl()/ntohl()` for 32-bit, `htonll()/ntohll()` for 64-bit. `htonll` is implemented in `protocol.c` using endianness detection. Conversion happens at send time (host → network) and receive time (network → host) for every multi-byte field.

**Q13: Why does `htonll` in `protocol.c` need a custom implementation?**  
A: POSIX does not standardize `htonll`. The custom implementation detects endianness at runtime by checking whether `*(char*)&num == 42` for `int num = 42`. On little-endian systems, it swaps the high and low 32-bit halves (each also byte-swapped by `htonl`). On big-endian systems, no conversion is needed.

**Q14: Can a malformed `TYPE_FILE_META` message crash the client?**  
A: No, after the fix. Before reading any fixed-offset fields, we validate `payload_len >= 14`. We also validate `14 + nlen <= payload_len` and `nlen < 1024` before the `memcpy` for the filename. If any check fails, we log an error and break the receive loop cleanly. No out-of-bounds memory access occurs.

---

## Remaining Limitations

1. **Server crash destroys checkpoints:** Sessions are in-memory only. A server restart forces all clients to restart from scratch (they receive `RESUME_REJECTED`).
2. **No per-chunk client ACK:** The server's checkpoint reflects bytes *sent*, not bytes *durably written* by the client. If the client writes data and then crashes before reconnecting, it will resume from the server's offset, which could be ahead of what the client actually committed to disk. This means no data loss (the server re-sends from its checkpoint), but potentially some redundant data written by the client on resumption (safe because offsets are explicit).
3. **No `fsync`:** File data is not explicitly flushed to disk per chunk. An OS crash (not a process crash) could lose buffered data. This is acceptable for the assignment's fault model.
4. **Linear session lookup:** The session list is scanned linearly. For very large numbers of concurrent sessions, a hash table would be more efficient. Not a concern at assignment scale.
5. **Topic table is static:** Adding a new topic after server startup requires a server restart.
