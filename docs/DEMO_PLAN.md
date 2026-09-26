# Final Demonstration Plan

## 1. Setup and Build
- **Action:** Open terminal in root project directory.
- **Command:** Run `make` in each folder or show that they build successfully.
- **Explain:** "We have 4 distinct versions exactly as requested. Part 1 (Basic), Part 2 Case 1 (No Session), Part 2 Case 2 (Session Management), and Part 2 Enhanced (Parallel TCP)."

## 2. Demonstrate Part 1
- **Action:** Start Server: `./server 8080 ../dataset`
- **Action:** Start Client: `./client 127.0.0.1 8080 Cane output`
- **Explain:** "The client connects, asks for topic 'Cane', and the server dynamically reads the directory and frames the files in our custom application protocol. Time and throughput are measured."

## 3. Demonstrate Fault Injection & Case 1
- **Action:** Start Server: `./server 8081 ../dataset 0.05`
- **Action:** Start Client: `./client 127.0.0.1 8081 Cane output`
- **Explain:** "The server injects a 5% failure probability per file chunk. You'll see the connection drop. The client detects the closed socket and restarts the transfer from 0."

## 4. Demonstrate Case 2 (Session Resume)
- **Action:** Start Server: `./server 8082 ../dataset 0.05`
- **Action:** Start Client: `./client 127.0.0.1 8082 Cane output`
- **Explain:** "Here, the client saves its exact byte offset to disk. When the connection drops, it sends a `RECONNECT_REQ` with its Session ID and offset. The server seeks exactly to that byte and resumes. Look at the redundant bytes metric—it's nearly zero despite the failures!"

## 5. Demonstrate Enhanced Version
- **Action:** Start Server: `./server 8083 ../dataset 0.05`
- **Action:** Start Client: `./client 127.0.0.1 8083 Cane output 4`
- **Explain:** "We implemented Parallel TCP Connections. The client spawns 4 worker threads. The server distributes the topic files across these connections. This vastly improves throughput and handles failures much faster, as shown by our benchmarks."

## 6. Show Graphs
- **Action:** Open `Results/Part2_Cases_throughput.png`
- **Explain:** "As the failure probability increases, Case 1 throughput drops to zero because it can never finish. Case 2 maintains stable throughput. The Enhanced version provides even higher base throughput."
