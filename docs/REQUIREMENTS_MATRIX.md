# Requirements Matrix

| ID | Requirement | Source | Mandatory? | Part/Case | Implementation | Test | Report |
|---|---|---|---|---|---|---|---|
| REQ-01 | Implement TCP socket-based server to map topics (keys) to absolute paths of directories | Part I | Yes | Part I | | | |
| REQ-02 | Client connects and issues a GET `<topic>` request | Part I | Yes | Part I | | | |
| REQ-03 | Server responds by sending all files belonging to the topic | Part I | Yes | Part I | | | |
| REQ-04 | Client creates directory corresponding to topic and populates it with received files | Part I | Yes | Part I | | | |
| REQ-05 | Implement single-threaded server serving clients sequentially | Part I | Yes | Part I | | | |
| REQ-06 | Implement multi-threaded server serving clients concurrently | Part I | Yes | Part I | | | |
| REQ-07 | Measure time required to send data using `clock_gettime()` or similar | Part I | Yes | Part I | | | |
| REQ-08 | Count data bytes sent on socket during transfer | Part I | Yes | Part I | | | |
| REQ-09 | Compute aggregate file transfer rate | Part I | Yes | Part I | | | |
| REQ-10 | Prepare table/graph of aggregate transfer rate vs number of clients | Part I | Yes | Part I | | | |
| REQ-11 | Implement fault injection: Server probabilistically terminates/interrupts active connection | Part II | Yes | Part II | | | |
| REQ-12 | Fault probability controlled by `<failure probability>` command-line arg | Part II | Yes | Part II | | | |
| REQ-13 | Server and client must remain correct and make forward progress despite disconnection | Part II | Yes | Part II | | | |
| REQ-14 | Case 1: No Session Management. No metadata retained. Restart from beginning | Part II | Yes | Case 1 | | | |
| REQ-15 | Case 2: With Session Management. Client and server maintain state (topic, file name, byte offset) | Part II | Yes | Case 2 | | | |
| REQ-16 | Case 2: Checkpoint state updated periodically on server, client, or both | Part II | Yes | Case 2 | | | |
| REQ-17 | Case 2: Client reports last known checkpoint upon reconnect. Server validates and resumes. | Part II | Yes | Case 2 | | | |
| REQ-18 | Measure and compare performance with and without session management (varying fault rate) | Part II | Yes | Case 1,2 | | | |
| REQ-19 | Implement Part-II Case 2 - Enhanced (e.g. Socket mode or Parallel TCP Connections) | Part II | Yes | Case 2 Enhanced | | | |
| REQ-20 | Server CLI: `./server <port> <topics_root_dir>` (Part 1) and `./server <port> <topics_root_dir> <failure probability>` (Part 2) | Interface | Yes | All | | | |
| REQ-21 | Client CLI: `./client <server_ip> <server_port> <topic> <output_dir>` | Interface | Yes | All | | | |
| REQ-22 | Client writes files under `output_dir/topic/` | Interface | Yes | All | | | |
| REQ-23 | `make` inside each Part folder produces `server` and `client` executables | Interface | Yes | All | | | |
| REQ-24 | `FinalSubmission_Group_<XXX>_<YYY>.tar.gz` or `.zip` structure | Submission | Yes | All | | | |
| REQ-25 | Design Document (Server data structures, message formats, fault injection, session protocol, enhancements, module structure, thought process) | Submission | Yes | All | | | |
| REQ-26 | Final Report (Part I, Part II Cases separated, Cover page, Design, Performance Analysis) | Submission | Yes | All | | | |
| REQ-27 | README file with build/run instructions for every case and fault-injection mechanism | Submission | Yes | All | | | |
| REQ-28 | Results folder with raw logs (`.csv`) and graphs (`.png`/`.pdf`) for each case | Submission | Yes | All | | | |
| REQ-29 | No hardcoded file path, port number, or IP address | Autograder | Yes | All | | | |
| REQ-30 | Server/Client must not prompt for interactive input | Autograder | Yes | All | | | |
