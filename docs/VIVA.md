# Viva Preparation Guide

**Q1: How does your application protocol handle partial TCP transfers?**
**Answer:** TCP is a byte stream, so `send()` and `recv()` are not guaranteed to transfer all requested bytes at once. We implemented `send_all()` and `recv_all()` loops that continue calling the socket API until the exact requested length is transferred, handling `EINTR` and partial counts cleanly. We also added a 5-byte header `[Length:4][Type:1]` to every message so the receiver knows exactly how many bytes to read for the payload.

**Q2: How does Case 2 (Session Management) guarantee no data corruption?**
**Answer:** The checkpoint truth is maintained strictly by the Client. The client only updates its internal offset *after* `fwrite` successfully writes the data to the local disk. When reconnecting, it sends this exact offset to the server. The server skips to this offset using `fseek` before sending more chunks.

**Q3: How does the server simulate faults?**
**Answer:** We implemented a probabilistic fault injector. Right before sending a chunk of data, the server generates a random float using `rand_r()`. If it falls below the `<failure probability>` argument, the server intentionally calls `close(client_sock)` and terminates the thread. This realistically simulates an abrupt TCP connection loss.

**Q4: How did you implement parallel connections in the Enhanced version?**
**Answer:** The client spawns $N$ pthreads. Each thread establishes its own TCP connection to the server and issues a `GET_REQUEST_WORKER` message containing its `worker_id` and the `total_workers`. The server uses modulo arithmetic `(file_index % total_workers) == worker_id` to only send a disjoint subset of the files to each connection. This avoids lock contention on both the client (different files) and server.

**Q5: Why did you use POSIX threads instead of select()/epoll()?**
**Answer:** The project explicitly required a "multi-threaded server". The Thread-Per-Client model is easier to reason about when injecting faults, as terminating a thread cleanly closes that specific client's state without affecting others.

**Q6: What happens if a client requests an invalid or malicious topic like `../etc/passwd`?**
**Answer:** The server performs basic path sanitization. If the requested topic contains `/` or `..`, the server immediately rejects the request with a `TYPE_ERROR` message and closes the connection.
