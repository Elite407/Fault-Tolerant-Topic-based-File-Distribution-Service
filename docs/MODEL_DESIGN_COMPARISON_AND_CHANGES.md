# Codebase Audit & Changes Based on Instructor Model Design
**Course:** IT305 LAB - Socket Programming Project  
**Reference Document:** `key-IT305-lab-socket-programming-project-assignment.pdf` ("Sample Design of Socket Programming Project")  
**Target Repository:** `Fault-Tolerant-Topic-based-File-Distribution-Service`  
**Group:** 100_113  

---

## Executive Summary

The reference document `key-IT305-lab-socket-programming-project-assignment.pdf` is the official **Model Solution / Reference Design** distributed by the course instructors. It addresses the evaluation criteria for the project's design and implementation across:
1. Topic-to-directory mapping data structures and startup initialization.
2. Application-layer protocol messages (`GET`, `OK`, `ERR`, `START`, `FILE_HDR`, `DATA`, `ACK`, `CLOSE`) and packet framing (Transaction ID).
3. Session state structure (`struct SessionState`) and session recovery workflow.
4. Fault injection and recovery semantics for Case 1 (clean restart with directory wipe) and Case 2 (resuming remaining files/offsets).

While the instructor explicitly noted on page 7:
> *"Note: We have seen few of your submissions, and some designs are really good – even better than this simplistic minimal model design."*

there are several concrete architectural, protocol, and algorithmic differences between the current repository implementation and the instructor's model design. 

This document details the exact differences, categorizes them into **Required/High-Priority Fixes**, **Protocol Realignment Changes**, and **Design Trade-offs**, and provides concrete code change recommendations for each component.

---

## 1. Topic-to-Directory Mapping (`struct TopicEntry`)

### Instructor Model Design Requirement (Page 1)
- The server must maintain an explicit data structure mapping topics to directory paths:
  ```c
  #define MAX_TOPIC_LEN 128
  #define MAX_PATH_LEN  512

  struct TopicEntry {
      char topic[MAX_TOPIC_LEN];
      char directory[MAX_PATH_LEN];
  };
  ```
- The mapping is stored in an array or linked list (`struct TopicEntry topic_table[MAX_TOPICS]`).
- **Initialization & Error Checking:** The server populates this table at server startup (scanning `topics_root_dir` or reading a configuration file). It must verify that the directories for each topic exist and are accessible (`access()` / `stat()`).
- Avoids hard-coding directory paths directly into server transfer routines.

### Current Implementation Status
- The server does not maintain an in-memory `TopicEntry` table.
- Instead, upon receiving a `GET` request, the server dynamically forms `<topics_root_dir>/<topic>` using `snprintf()` and invokes `scandir()`.
- Topic validity is checked solely by verifying string characters (`..`, `/`) and checking if `scandir()` returns `< 0`.

### Changes Needed in Code
In `Part1/server.c`, `Part2_Case1/server.c`, `Part2_Case2/server.c`, and `Part2_Case2_Enhanced/server.c`:
1. **Define `struct TopicEntry`:**
   ```c
   typedef struct {
       char topic[128];
       char directory[512];
   } TopicEntry;

   #define MAX_TOPICS 64
   TopicEntry topic_table[MAX_TOPICS];
   int topic_count = 0;
   ```
2. **Add a startup initialization function `init_topic_table(const char *root_dir)`:**
   - Scan `root_dir` using `opendir()` / `readdir()`.
   - For every directory entry (excluding `.` and `..`), construct its absolute or relative path.
   - Use `stat()` and `access(path, R_OK | X_OK)` to ensure it is a valid, readable directory.
   - Populate `topic_table[topic_count++]`.
3. **Lookup routine `find_topic_directory(const char *topic)`:**
   - Instead of formatting strings on the fly, search `topic_table` for a match.
   - If not found, immediately send `ERR(unknown_topic)`.

---

## 2. Protocol Header and Message Framing

### Instructor Model Design Requirement (Page 2)
The application layer header must at minimum contain:
`Header | Payload`
- **Message Type**
- **Transaction ID:** Used to match responses to requests (e.g. `uint32_t transaction_id`)
- **Payload Length:** Size of following payload

#### Model Message Types:
| Message Type | Direction | Purpose / Remark |
| :--- | :--- | :--- |
| `GET <topic>` | Client $\to$ Server | Request topic metadata and file list. In Case 2, includes `session_id`. |
| `OK <directory, file_list>` | Server $\to$ Client | Informs client of files and sequence. In Case 2, returns `session_id`. |
| `ERR <error_code, msg>` | Server $\to$ Client | Inform client of errors with specific error code. |
| `START <topic>` | Client $\to$ Server | Client requests file transfer after receiving `OK` and creating directory. |
| `FILE_HDR <topic, filename, attrs>` | Server $\to$ Client | Sent before each file transfer (file size, permissions, remaining offset). |
| `DATA <hdr: topic, file, offset, size> <chunk>` | Server $\to$ Client | Carries file chunk data. |
| `ACK <hdr: tx_id> <topic, file, offset>` | Client $\to$ Server | Acknowledges receipt of `FILE_HDR` or `DATA` chunk. |
| `CLOSE` | Server $\to$ Client | Sent when topic transfer completes. |

### Current Implementation Status
In `protocol.h`:
```c
typedef struct {
    uint32_t length; // Length of payload
    uint8_t type;    // Message Type
} msg_header_t;
```
- **Transaction ID is missing** from `msg_header_t`.
- **No `START` message:** When the client sends `TYPE_GET_REQUEST`, the server sends `TYPE_TOPIC_RESPONSE` and immediately blasts all file chunks.
- **`TOPIC_RESPONSE` is incomplete:** It only sends 1-byte status (`STATUS_FOUND`) and 4-byte total file count. It does not send the list of file names or directory name.
- **No `CLOSE` message:** The server sends `TYPE_TOPIC_EOF` (type `0x07`, length 0).
- **No `ACK` messages:** Data transfer is unidirectional without receiver ACKs.

### Changes Needed in Code
1. **Update `protocol.h` Header:**
   ```c
   #pragma pack(push, 1)
   typedef struct {
       uint8_t  type;           // Message type
       uint32_t transaction_id; // Match request-response
       uint32_t length;         // Payload length (network byte order)
   } msg_header_t;
   #pragma pack(pop)
   ```
2. **Align Message Constants in `protocol.h`:**
   ```c
   #define MSG_GET       0x01
   #define MSG_OK        0x02
   #define MSG_ERR       0x03
   #define MSG_START     0x04
   #define MSG_FILE_HDR  0x05
   #define MSG_DATA      0x06
   #define MSG_ACK       0x07
   #define MSG_CLOSE     0x08
   ```
3. **Add `START` Handshake:**
   - Server: after sending `OK`, wait for `MSG_START` before sending files.
   - Client: upon receiving `OK`, create output directory, then send `MSG_START`.
4. **Replace `TOPIC_EOF` with `CLOSE`:**
   - Server sends `MSG_CLOSE` on topic completion, and client cleanly exits the transfer loop on receiving `MSG_CLOSE`.

---

## 3. Part 1: Single-Threaded vs Multi-Threaded Execution

### Instructor Model Design Requirement (Page 2 & 3)
- **Single-Threaded Server:** Accepts one client, runs transfer to completion (`CLOSE`), closes socket, then accepts the next client.
- **Multi-Threaded Server:** Main thread accepts connections and immediately spawns a worker thread (`pthread_create`) per client.

### Current Implementation Status
- `Part1/server.c` accepts an optional 4th CLI parameter: `./server <port> <topics_root_dir> [single|multi]`.
- Defaults to `multi` if the 4th parameter is omitted.
- Note: The assignment specification on Page 4 states the exact autograder invocation is `./server <port> <topics_root_dir>`.

### Changes Needed in Code
- To strictly conform to automated tests that test single-threaded vs multi-threaded modes:
  - Keep the default behavior robust.
  - Optionally support an environment variable or flag (e.g. `SERVER_MODE=single`) or compile two distinct targets in `Part1/Makefile`:
    - `server` (multi-threaded, standard for autograder)
    - `server_single` (strictly single-threaded for comparative benchmarking)

---

## 4. Part 2 Case 1: No Session Management (Handling Disconnection)

### Instructor Model Design Requirement (Page 4 & 5)
1. **Server Fault Injection:**
   - Random decision after each chunk transmitted:
     ```c
     if (random_failure_event(failure_probability)) {
         close(connection);
         return;
     }
     ```
2. **Client Crash Recovery (CRITICAL DIFFERENCE):**
   - The model design explicitly requires:
     > *"• In the client program:*  
     > *◦ add detection of connection closure*  
     > *◦ **Remove the topic directory***  
     > *◦ re-establish a fresh connection with the server and request the topic again"*
   - In pseudo-code (Page 5):
     ```text
     IF connection closed:
         Close local file
         attempt_failed = TRUE
         BREAK
     IF attempt_failed:
         Remove local files / topic directory
         BREAK
     ```

### Current Implementation Status
- `Part2_Case1/server.c` injects faults inside the chunk loop (matches specification).
- `Part2_Case1/client.c` detects disconnect and loops, but **does NOT remove the topic directory or delete partially downloaded files**.
- The client only overwrites files when re-opening them with `"wb"`. If a crash occurs on file 5 of 10, files 1 through 4 are still left on disk from the previous attempt, violating the strict requirement to *"Remove the topic directory and restart from scratch"*.

### Changes Needed in Code
In `Part2_Case1/client.c`:
1. **Implement Directory Cleanup Utility:**
   ```c
   void clean_topic_dir(const char *topic_dir) {
       DIR *d = opendir(topic_dir);
       if (!d) return;
       struct dirent *p;
       while ((p = readdir(d)) != NULL) {
           if (strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0) continue;
           char filepath[4096];
           snprintf(filepath, sizeof(filepath), "%s/%s", topic_dir, p->d_name);
           unlink(filepath);
       }
       closedir(d);
       rmdir(topic_dir);
   }
   ```
2. **Invoke Cleanup on Disconnect:**
   When `error_occurred == 1` or `recv <= 0`:
   ```c
   if (!success && error_occurred) {
       reconnects++;
       clean_topic_dir(topic_dir); // Wipe directory as required by model key!
       mkdir_p(topic_dir);
       usleep(100000);
   }
   ```

---

## 5. Part 2 Case 2: Session Management Protocol

### Instructor Model Design Requirement (Page 6 & 7)

#### 1. Session State Representation:
```c
struct SessionState {
    uint64_t session_id;
    char topic[MAX_TOPIC_LEN];
    int file_index; // or char filename[MAX_FILENAME_LEN]
    uint64_t file_offset;
    time_t last_update;
    // other information for record keeping
};
```

#### 2. Protocol Sequence:
- **First Connection:**
  - Client sends `GET <topic>`.
  - Server generates `SessionState`, stores it in a session table, and sends `session_id` inside `OK`.
  - Client sends `START <topic>` containing `session_id`.
  - Server sends files sequentially.
  - Server updates `SessionState` (`file_index`, `file_offset`, `last_update`).
- **On Reconnection after Failure:**
  - Client connects and sends `GET <topic, session_id>`.
  - Server looks up `session_id` in session table.
  - Server responds with `OK` containing **only the list of remaining files**.
  - Client sends `START` with `session_id`.
  - Server resumes transfer:
    - Sends `FILE_HDR` with **remaining file size only** (`file_size - restored_offset`).
    - `offset = restored_offset`.
  - Client receives `FILE_HDR`, opens the file, and performs `fseek(f, offset, SEEK_SET)`.

### Current Implementation Status
In `Part2_Case2`:
1. **Server-Side Session State:**
   - Server only stores `session_id` and `topic` in its linked list.
   - It **does not store** `file_index` or `file_offset` on the server!
2. **Client-Driven Resume:**
   - Current implementation relies on a custom message `TYPE_RECONNECT_REQ (0x09)` where the client tells the server `(session_id, current_file_idx, current_offset)`.
   - The server validates the `session_id` and replies with `TYPE_RESUME_RESPONSE (0x0A)`.
   - The server then iterates all files, skipping `< resume_file_idx`.
   - For `resume_file_idx`, server sends `FILE_META` with **total file size** (not remaining file size).

### Architectural Analysis & Comparison
- **Trade-off Highlighted in Design Document:**
  In `FinalSubmission_Group_100_113/Design_Document_Group_100_113.md`, the team argued:
  > *"Storing byte offsets on the server would require large, frequently updated data structures, leading to heavy mutex contention and potential memory leaks... We opted for a client-driven offset model. The server merely tracks the active session_id for authorization, while the client dictates the exact byte offset to resume from."*
- **Instructor's Note on Page 7:**
  > *"Note: We have seen few of your submissions, and some designs are really good – even better than this simplistic minimal model design."*
- The client-driven approach is stateless on the server, avoiding out-of-sync state when packet losses occur mid-stream. However, to align with the model solution or improve session tracking:

### Changes Needed in Code (Case 2)
1. **Expand `Session` structure in `server.c`:**
   ```c
   typedef struct Session {
       uint64_t session_id;
       char topic[256];
       uint32_t current_file_idx;
       uint64_t current_offset;
       time_t last_update;
       struct Session *next;
   } Session;
   ```
2. **Update Server-Side Checkpoint:**
   In `Part2_Case2/server.c`, update `s->current_file_idx = file_index` and `s->current_offset = offset` as chunks are transmitted or acknowledged.
3. **Unified `GET` Message with Session ID:**
   Instead of having a separate `TYPE_RECONNECT_REQ`:
   - Send `TYPE_GET_REQUEST` with optional `session_id` (0 for new, non-zero for resume).
   - Server returns `TYPE_TOPIC_RESPONSE` containing `session_id` and status.
4. **Accurate Remaining Size in `FILE_HDR` on Resume:**
   When resuming a file at `resume_offset`, send the remaining size or include `offset` in the header so the client knows exactly how many bytes are left to transfer.

---

## 6. Autograder CLI Compatibility Verification

The assignment specification imposes strict CLI rules (Page 4 of `IT305-lab-socket-programming-project-assignment.pdf`):
- Server:
  - Part 1: `./server <port> <topics_root_dir>`
  - Part 2: `./server <port> <topics_root_dir> <failure probability>`
- Client:
  - `./client <server_ip> <server_port> <topic> <output_dir>`
- Output Directory Structure:
  - Client must create `<output_dir>/<topic>/` and save files inside it.

### Current Implementation Status
- `Part1/server.c` accepts `<port> <topics_root_dir> [single|multi]`. Compatible with standard invocation.
- `Part2_Case1/server.c` and `Part2_Case2/server.c` accept `<port> <topics_root_dir> <failure probability>`. Exactly compatible.
- All client implementations accept `<server_ip> <server_port> <topic> <output_dir>`. Exactly compatible.
- Destination folder created via `mkdir_p(output_dir/topic)`. Compatible.

---

## 7. Action Plan & Code Changes Checklist

| Priority | Component | File | Proposed Change | Reason |
| :---: | :--- | :--- | :--- | :--- |
| **High** | Client Cleanup | `Part2_Case1/client.c` | Add `clean_topic_dir()` to wipe directory on disconnect before reconnecting. | Explicitly required in model design (Page 4-5) |
| **High** | Topic Data Structures | `Part1/server.c`<br>`Part2_Case1/server.c`<br>`Part2_Case2/server.c` | Add `struct TopicEntry`, populate table at startup, validate with `access()` / `stat()`. | Evaluated in rubric (2 marks, Page 8) |
| **Medium** | Protocol Framing | `*/protocol.h` | Add `transaction_id` (4 bytes) to `msg_header_t`. | Required by model header specification (Page 2) |
| **Medium** | Protocol Handshake | `*/server.c`<br>`*/client.c` | Send file list in `OK`, add `START` handshake from client before sending file data. | Matches model sequence diagrams (Pages 3, 5, 7) |
| **Medium** | Session Tracking | `Part2_Case2/server.c` | Store `current_file_idx`, `current_offset`, and `last_update` inside `struct Session`. | Evaluated in rubric (10 marks, Page 8) |
| **Low** | Message Naming | `*/protocol.h` | Rename `TYPE_TOPIC_EOF` to `TYPE_CLOSE` / `MSG_CLOSE`. | Alignment with model message terminology |

---

## 8. Viva & Evaluation Strategy

When presenting the project or answering viva questions regarding deviations from the sample key:
1. **Explain the Topic Mapping:** If using the in-memory `TopicEntry` table initialized at startup, highlight that it avoids hardcoding while preventing dynamic path traversal attacks.
2. **Defend the Session Checkpoint Design:** 
   - Point out that while the sample model uses server-side tracking via ACKs, the **client-driven offset protocol** avoids server state corruption during abrupt TCP RSTs and eliminates mutex contention in high-concurrency environments.
   - Reference the instructor's note on Page 7: *"some designs are really good – even better than this simplistic minimal model design."*
3. **Emphasize Case 1 Directory Wipe:** Demonstrate that Case 1 completely purges partial files upon disconnect to highlight the true overhead of having no session management compared to Case 2.
