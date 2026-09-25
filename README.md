# Fault-Tolerant Topic-Based File Distribution Service

This repository contains the implementation of a fault-tolerant, topic-based file distribution service using TCP sockets in C/C++. The system distributes image files (from the Animals10 dataset) over TCP, organizing them into topics based on animal categories.

## Project Structure

As required by the project specifications, the implementation is divided into incremental parts:

*   **`Part1/`**: Basic Topic-Based File Distribution. Contains both a single-threaded and a multi-threaded server implementation that maps topics to directories and serves all files under that topic upon a `GET` request.
*   **`Part2_Case1/`**: Fault Injection (No Session Management). Adds a probabilistic fault-injection module to the multi-threaded server. If a connection drops, the client must restart the transfer from scratch.
*   **`Part2_Case2/`**: Session Management. Implements a checkpoint-and-resume protocol. The client and server track progress, allowing a dropped connection to resume from the last 64 KB chunk boundary, significantly reducing redundant data transfer.
*   **`Part2_Case2_Enhanced/`**: Parallel TCP Connections. An enhancement over Case 2 where the client spawns multiple concurrent sockets to download different files within the same topic simultaneously, improving aggregate throughput.
*   **`Results/`**: Contains raw performance logs (`.csv`) and generated graphs (`.png`/`.pdf`) analyzing the throughput, completion time, and fault-recovery overhead of the different implementations.

## Dependencies
*   Linux/POSIX environment (or WSL on Windows)
*   GCC or Clang compiler
*   `make`

## Build Instructions

Each case directory acts as an independent project with its own `Makefile`. To build a specific case:

```bash
cd Part1/  # or Part2_Case1/, etc.
make
```

This will produce two executables in the folder: `server` and `client`.

## Running the System

### Server

The server requires specific command-line arguments. Note that Part 2 cases require an additional `<failure probability>` argument.

**Part 1:**
```bash
./server <port> <topics_root_dir>
```

**Part 2 (All Cases):**
```bash
./server <port> <topics_root_dir> <failure probability>
```
*(where `<failure probability>` is a float between `0.0` and `1.0` specifying how frequently the server interrupts an active session).*

### Client

The client connects to the server and fetches a specific topic:

```bash
./client <server_ip> <server_port> <topic> <output_dir>
```

*(For `Part2_Case2_Enhanced`, an optional trailing argument for the number of parallel connections `<N>` may be supported).*

## Dataset

This project is designed to run against the [Animals10 dataset](https://www.kaggle.com/datasets/alessiocorrado99/animals10). Ensure the dataset is extracted and the path to its root directory is provided to the server via `<topics_root_dir>`.

## Academic Integrity
This is a group project submission for IT305: Socket Programming. All code and report content represent the group's own work.