# Visual Asset Catalog & Curation Index
**Total Visual Assets:** 69 files in `Images/`

## 1. Executive Terminal Proof Cards (Dual-Pane High-Resolution)
These 6 cards provide crisp, dark-themed, publication-ready dual-pane views pairing server daemons with client subscribers.

| File | Resolution | Size | Flow Description |
|---|---|---|---|
| `Images/terminal_broker_startup.png` | 1520x860 | 97 KB | Server Daemon Startup, Topic Catalog Indexing (10 Topics), Socket Listen State |
| `Images/terminal_part1_transfer.png` | 1520x860 | 156 KB | Part 1 Basic Multi-Threaded Topic File Distribution (12 files, 52.6 MB/s) |
| `Images/terminal_part2_case1_failover.png` | 1520x860 | 135 KB | Part 2 Case 1 Fault Drop Detection, Recursive Directory Wipe & Restart |
| `Images/terminal_part2_case2_session_resume.png` | 1520x860 | 159 KB | Part 2 Case 2 Checkpoint Session Resumption from Byte Offset (Zero Corrupt/Redundant) |
| `Images/terminal_part2_enhanced_parallel.png` | 1520x860 | 140 KB | Part 2 Case 2 Enhanced 4-Worker Multi-TCP Parallel Streaming (5.2 MB/s, 4x Speedup) |
| `Images/terminal_tests_verification.png` | 1520x860 | 101 KB | Automated End-to-End Integration Test Suite 15/15 PASS Verification |

## 2. Benchmark Visual Graphs (300 DPI Publication Plots)
Generated in `Results/` from empirical sweep metrics.

| File | Resolution | Description |
|---|---|---|
| `Results/Part1_throughput.png` | 2656x1619 | Multi-client throughput scalability curve |
| `Results/Part2_Cases_throughput.png` | 2820x1619 | Case 1 vs Case 2 throughput degradation under fault injection |
| `Results/Part2_Enhanced_throughput.png` | 2655x1619 | Concurrency throughput speedup vs parallel TCP worker connections |

## 3. Part 1 Empirical Client Terminal Captures (14 files)
Empirical multi-client physical lab testing across 1, 2, and 3 concurrent client nodes.

| File | Resolution | Size |
|---|---|---|
| `Images/C1_Part1_Multi_thread_1c.png` | 3200x454 | 60 KB |
| `Images/C1_Part1_Multi_thread_2c.png` | 3196x448 | 59 KB |
| `Images/C1_Part1_Single_Thread_1c.png` | 3200x918 | 147 KB |
| `Images/C2_Part1_Single_Thread.png` | 3200x1604 | 236 KB |
| `Images/C2_Part1_Single_Thread_1c.png` | 3200x918 | 147 KB |
| `Images/C2_Part1_Single_Thread_2c.png` | 3200x1604 | 236 KB |
| `Images/C2_Part1_Single_thread_3c.png` | 3200x462 | 61 KB |
| `Images/C2_part1_Multi_thread_3c.png` | 3200x404 | 51 KB |
| `Images/c1_Part1_MultiThread_3c.png` | 1401x211 | 44 KB |
| `Images/c1_Part1_SingleThread_2c.png` | 1401x748 | 199 KB |
| `Images/c1_Part1_SingleThread_3c.png` | 1401x211 | 44 KB |
| `Images/c2_Part1_MultiThread_2c.png` | 1401x211 | 44 KB |
| `Images/c3_Part1_MultiThread_3c.png` | 1401x205 | 43 KB |
| `Images/c3_Part1_SingleThread_3c.png` | 1401x211 | 43 KB |

## 4. Part 2 Case 1 Empirical Terminal Captures (4 files)
Empirical captures of unmanaged disconnect drops and directory wipe cycles under P in {0.05, 0.1, 0.5}.

| File | Resolution | Size |
|---|---|---|
| `Images/C1_Part2_Case1_p=0.05_1c.png` | 3200x1848 | 296 KB |
| `Images/C1_Part2_case1_p=0.1_1c.png` | 3196x1132 | 152 KB |
| `Images/C2_Part2_case1_p=0.5_2c.png` | 3200x1364 | 189 KB |
| `Images/c1_Part2_Case1_p=0.5_2c.png` | 1401x978 | 232 KB |

## 5. Part 2 Case 2 Stateful Session Checkpointing Captures (17 files)
Empirical captures of session resumption, byte offset tracking, and 0 redundant retransmissions.

| File | Resolution | Size |
|---|---|---|
| `Images/C1_Part2_case2_p=0.1_1c.png` | 3196x1132 | 152 KB |
| `Images/C2_Part2_case2_p=0.05_2c(1).png` | 3200x1588 | 261 KB |
| `Images/C2_Part2_case2_p=0.05_2c(2).png` | 3176x1984 | 321 KB |
| `Images/c1_Part2_Case2_p=0.01_1c(1).png` | 1441x933 | 318 KB |
| `Images/c1_Part2_Case2_p=0.01_1c(2).png` | 1441x933 | 290 KB |
| `Images/c1_Part2_Case2_p=0.01_2c(1).png` | 1441x887 | 310 KB |
| `Images/c1_Part2_Case2_p=0.01_2c(2).png` | 1441x887 | 264 KB |
| `Images/c1_Part2_Case2_p=0.05_1c(1).png` | 1401x1000 | 319 KB |
| `Images/c1_Part2_Case2_p=0.05_1c(2).png` | 1401x977 | 269 KB |
| `Images/c1_Part2_Case2_p=0.05_2c(1).png` | 1401x918 | 286 KB |
| `Images/c1_Part2_Case2_p=0.05_2c(2).png` | 1401x956 | 267 KB |
| `Images/c1_Part2_Case2_p=0.1_1c(1).png` | 1401x885 | 292 KB |
| `Images/c1_Part2_Case2_p=0.1_1c(2).png` | 1401x912 | 251 KB |
| `Images/c1_Part2_Case2_p=0.1_2c(1).png` | 1401x912 | 261 KB |
| `Images/c1_Part2_Case2_p=0.1_2c(2).png` | 1401x912 | 257 KB |
| `Images/c2_part2_case2_p=0.01_2c(1).png` | 1095x772 | 241 KB |
| `Images/c2_part2_case2_p=0.01_2c(2).png` | 1113x791 | 232 KB |

## 6. Part 2 Case 2 Enhanced Parallel Streaming Captures (28 files)
Empirical captures of 4 concurrent worker threads with modulo workload partitioning.

| File | Resolution | Size |
|---|---|---|
| `Images/Screenshot 2026-10-27 000645.png` | 3200x1522 | 205 KB |
| `Images/Screenshot 2026-10-27 023305.png` | 3200x1960 | 289 KB |
| `Images/c1_Part2_Case2_En_p=0.01_1c(1).png` | 1441x887 | 377 KB |
| `Images/c1_Part2_Case2_En_p=0.01_1c(2).png` | 1441x887 | 324 KB |
| `Images/c1_Part2_Case2_En_p=0.01_2c(1).png` | 1441x868 | 369 KB |
| `Images/c1_Part2_Case2_En_p=0.01_2c(2).png` | 1441x868 | 318 KB |
| `Images/c1_Part2_Case2_En_p=0.01_2c(3).png` | 1821x980 | 276 KB |
| `Images/c1_Part2_Case2_En_p=0.05_1c(1).png` | 1821x826 | 354 KB |
| `Images/c1_Part2_Case2_En_p=0.05_1c(2).png` | 1821x826 | 307 KB |
| `Images/c1_Part2_Case2_En_p=0.05_1c(3).png` | 1822x980 | 250 KB |
| `Images/c1_Part2_Case2_En_p=0.05_2c(1).png` | 1822x870 | 374 KB |
| `Images/c1_Part2_Case2_En_p=0.05_2c(2).png` | 1822x864 | 319 KB |
| `Images/c1_Part2_Case2_En_p=0.05_2c(3).png` | 1822x929 | 241 KB |
| `Images/c1_Part2_Case2_En_p=0.1_1c(1).png` | 1822x932 | 404 KB |
| `Images/c1_Part2_Case2_En_p=0.1_1c(2).png` | 1822x932 | 355 KB |
| `Images/c1_Part2_Case2_En_p=0.1_1c(3).png` | 1822x1005 | 258 KB |
| `Images/c1_Part2_Case2_En_p=0.1_2c(1).png` | 1822x865 | 375 KB |
| `Images/c1_Part2_Case2_En_p=0.1_2c(2).png` | 1822x865 | 328 KB |
| `Images/c1_Part2_Case2_En_p=0.1_2c(3).png` | 1822x995 | 263 KB |
| `Images/c2_part2_case2_en_p=0.01_2c(1).png` | 1449x660 | 283 KB |
| `Images/c2_part2_case2_en_p=0.01_2c(2).png` | 1461x677 | 235 KB |
| `Images/c2_part2_case2_en_p=0.01_2c(3).png` | 1768x815 | 238 KB |
| `Images/c2_part2_case2_en_p=0.05_2c(1).png` | 1784x720 | 313 KB |
| `Images/c2_part2_case2_en_p=0.05_2c(2).png` | 1186x722 | 257 KB |
| `Images/c2_part2_case2_en_p=0.05_2c(3).png` | 1840x804 | 217 KB |
| `Images/c2_part2_case2_en_p=0.1_2c(1).png` | 1858x919 | 392 KB |
| `Images/c2_part2_case2_en_p=0.1_2c(2).png` | 1858x960 | 350 KB |
| `Images/c2_part2_case2_en_p=0.1_2c(3).png` | 1858x960 | 256 KB |

## 7. Miscellaneous Historical Verification Captures (0 files)
| File | Resolution | Size |
|---|---|---|
