# Fault Tolerant Topic-based File Distribution Service.

## 1. Project Overview
This project implements a custom TCP socket-based file distribution system in C. It supports single/multi-threaded servers, a custom application protocol for framing binary file transfers, and robust fault-tolerance mechanisms including session management and parallel TCP connections.

## 2. Architecture
The architecture is based on POSIX pthreads and standard BSD sockets.
- **Part 1:** Basic multi-threaded TCP transfer.
- **Part 2 Case 1:** Fault-injected environment without session state (restarts on failure).
- **Part 2 Case 2:** Fault-injected environment with session state. The client checkpoints bytes to disk and sends a `RECONNECT_REQ` to resume transfers precisely.
- **Part 2 Case 2 Enhanced:** Uses multiple parallel TCP connections to distribute the load across worker threads for much higher throughput.

## 3. Prerequisites
- `gcc` compiler
- Linux/POSIX environment
- `make`

## 4. Compilation
Change into any of the Part directories and run `make`:
```bash
cd Part1
make
```
This produces `server` and `client` executables. Repeat for `Part2_Case1`, `Part2_Case2`, and `Part2_Case2_Enhanced`.

## 5. Execution

### Part 1
**Server:**
`./server <port> <topics_root_dir> [single|multi]`
Example: `./server 8080 ../dataset`

**Client:**
`./client <server_ip> <server_port> <topic> <output_dir>`
Example: `./client 127.0.0.1 8080 Cane output`

### Part 2 (All Cases)
**Server:**
`./server <port> <topics_root_dir> <failure probability>`
*The failure probability is evaluated repeatedly during file transfer to simulate sudden TCP connection drops.*
Example: `./server 8081 ../dataset 0.05` (5% chance of dropping connection per chunk)

**Client:**
`./client <server_ip> <server_port> <topic> <output_dir>`
*(For the Enhanced version, an optional 5th parameter specifies the number of parallel connections, defaulting to 4).*
Example: `./client 127.0.0.1 8081 Cane output 4`

## 6. Benchmarking & Results
A benchmarking script `benchmark/run_benchmarks.sh` automates testing and performance measurement.
- Results are saved as `.csv` files in the `Results/` folder.
- Graphs are generated automatically using `benchmark/plot.py`.

## 7. Troubleshooting
- If the server complains about port binding, ensure it was gracefully killed (or use a different port). Socket `SO_REUSEADDR` is implemented to minimize this.
- If the client stalls, check if the `<topics_root_dir>` is correct on the server.
