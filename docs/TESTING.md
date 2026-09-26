# Testing Strategy

## 1. Test Environments
Tests will be executed locally via loopback (`127.0.0.1`) and, if possible, across multiple machines to validate true network behavior.

## 2. Test Cases
| ID | Category | Description | Expected Outcome |
|----|----------|-------------|------------------|
| TC01 | Basic | Valid topic `Cane` requested | All files transferred correctly. |
| TC02 | Basic | Invalid topic requested | Client receives error and exits cleanly. |
| TC03 | Concurrency | 5 clients request topics simultaneously | Multi-threaded server handles all without data corruption. |
| TC04 | Fault | Server failure probability `0.05` (Case 1) | Connection drops, client reconnects, transfer restarts and eventually finishes. |
| TC05 | Fault | Server failure probability `0.10` (Case 2) | Connection drops, client reconnects, session verified, transfer resumes from offset, finishes successfully. |
| TC06 | Edge Case | Client killed (`SIGINT`) mid-transfer | Server detects connection reset, cleans up thread resources without crashing. |
| TC07 | Performance| Vary clients (1, 2, 4, 8) in Part 1 | Multi-threaded throughput should scale better than single-threaded. |
| TC08 | Performance| Parallel TCP Connections (Enhanced) | Throughput increases with 2-4 connections compared to standard Case 2. |

## 3. Automation Scripts
We will create a `benchmark/` folder containing bash/Python scripts to:
1. Start the server in the background.
2. Launch $N$ clients in parallel.
3. Capture timestamps.
4. Calculate aggregate throughput.
5. Save results to `Results/*.csv`.
