# Autograder Checklist

This checklist contains all requirements that are critical for automated evaluation.
If any of these fail, the autograder will reject or deduct marks for the submission.

## 1. Directory Structure
- [ ] Submission is a compressed archive named `FinalSubmission_Group_<XXX>_<YYY>.zip` or `.tar.gz`
- [ ] Root folder inside archive is `FinalSubmission_Group_<XXX>_<YYY>/`
- [ ] Contains `FinalReport_Group_<XXX>_<YYY>.pdf`
- [ ] Contains `README.md`
- [ ] Contains `Part1/`
- [ ] Contains `Part2_Case1/`
- [ ] Contains `Part2_Case2/`
- [ ] Contains `Part2_Case2_Enhanced/`
- [ ] Contains `Results/`

## 2. Compilation
- [ ] Running `make` inside `Part1/` produces executables exactly named `server` and `client`
- [ ] Running `make` inside `Part2_Case1/` produces executables exactly named `server` and `client`
- [ ] Running `make` inside `Part2_Case2/` produces executables exactly named `server` and `client`
- [ ] Running `make` inside `Part2_Case2_Enhanced/` produces executables exactly named `server` and `client`
- [ ] Executables are directly runnable (no extra steps required after `make`)

## 3. Server CLI Invocation
- [ ] Part 1 Server can be invoked as: `./server <port> <topics_root_dir>`
- [ ] Part 2 (all cases) Server can be invoked as: `./server <port> <topics_root_dir> <failure probability>`
- [ ] Server does NOT require any extra mandatory arguments
- [ ] Optional arguments (if any) have sensible defaults so the standard invocation works as-is

## 4. Client CLI Invocation
- [ ] Client can be invoked as: `./client <server_ip> <server_port> <topic> <output_dir>`
- [ ] Client does NOT require any extra mandatory arguments
- [ ] Optional arguments (if any) have sensible defaults so the standard invocation works as-is

## 5. File System Behavior
- [ ] The `topics_root_dir` argument is expected to contain one sub-folder per topic (e.g. `topics_root_dir/cane/`)
- [ ] Client correctly fetches the topic and writes received files to `output_dir/topic/`
- [ ] Client creates the `output_dir/topic/` directory if it does not exist

## 6. Hardcoding Prohibitions
- [ ] NO hardcoded IP addresses (e.g. `127.0.0.1` must be passed via CLI)
- [ ] NO hardcoded port numbers (e.g. `8080` must be passed via CLI)
- [ ] NO hardcoded file paths (e.g. `/home/manish/...` must be passed via CLI)
- [ ] NO interactive prompts (e.g. no `printf("Enter port: "); scanf(...)`)

## 7. Results Folder
- [ ] `Results/` contains raw `.csv` files for performance logs
- [ ] `Results/` contains graphs (e.g. `.png` or `.pdf`) for each case
- [ ] Example names used: `Partname_CaseName_throughput.png` etc.
