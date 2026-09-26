# Design Review Matrix

| Requirement | Design Section | Fully Covered? | Potential Issue | Resolution |
|-------------|----------------|----------------|-----------------|------------|
| Implement TCP socket-based server to map topics | 5, 8 | YES | Path traversal vulnerability | Added path sanitization |
| Client connects and issues a GET `<topic>` request | 7, 9 | YES | None | - |
| Server responds by sending all files | 10 | YES | Large files causing memory issues | Send in 8KB chunks |
| Client creates directory and populates it | 7, 26 | YES | Directory already exists | Ensure `mkdir -p` logic is used |
| Single-threaded server | 11 | YES | Clients block others indefinitely | Expected behavior |
| Multi-threaded server | 12 | YES | Thread resource leaks | Use `pthread_detach` |
| Measure time with `clock_gettime()` | 22 | YES | Clock precision | Use `CLOCK_MONOTONIC` |
| Count data bytes sent | 23 | YES | Needs explicit socket metric tracking | Track return value of `send_message` |
| Compute aggregate file transfer rate | 23 | YES | None | - |
| Prepare table/graph vs number of clients | 34, Testing | YES | Manual vs Automated | Write `benchmark.sh` scripts |
| Fault injection controlled by `<failure probability>` | 14 | YES | When exactly to inject? | Inject right before chunk transmission |
| Case 1: No Session Management | 15 | YES | Client hangs on dropped connection | Handle socket errors/0-byte reads cleanly |
| Case 2: With Session Management | 16, 17 | YES | Server runs out of memory for sessions | Implement session cleanup/timeout if needed |
| Case 2 Checkpoint state | 18 | YES | Server vs Client truth | Rely on Client truth (bytes written to disk) |
| Case 2 Client reports checkpoint, Server validates | 19 | YES | Stale/corrupt sessions | Server strictly validates offset/index |
| Part-II Case 2 - Enhanced | 20 | YES | Parallel connections add sync complexity | Assign disjoint files to workers to avoid lock contention |
| Required Server/Client CLI | 4 | YES | Optional args breaking standard usage | Optional args handled gracefully at end |
| `make` produces `server` and `client` | 27 | YES | None | - |

**Verdict:** The design meets all mandatory requirements from the PDF and addresses edge cases cleanly. We are approved to proceed to implementation.
