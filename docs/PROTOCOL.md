# Application Protocol Design

## 1. Framing and Base Header
TCP is a byte stream, so all messages must be explicitly framed.
Every message begins with a fixed-size header:
`| Message Length (4 bytes, uint32_t, network byte order) | Message Type (1 byte, uint8_t) | Payload (Length bytes) |`

The `Message Length` specifies the number of bytes in the `Payload`.

## 2. Message Types

### PART I
- `0x01: GET_REQUEST`
  - Direction: Client -> Server
  - Payload: Topic name (null-terminated string).
- `0x02: TOPIC_RESPONSE`
  - Direction: Server -> Client
  - Payload: Status code (1 byte: `0`=Not Found, `1`=Found), followed by Total Files (4 bytes, uint32_t) if found.
- `0x03: FILE_META`
  - Direction: Server -> Client
  - Payload: File Index (4 bytes, uint32_t), File Size (8 bytes, uint64_t), Filename Length (2 bytes, uint16_t), Filename (string).
- `0x04: FILE_CHUNK`
  - Direction: Server -> Client
  - Payload: File Index (4 bytes, uint32_t), Chunk Offset (8 bytes, uint64_t), Chunk Data (variable).
- `0x05: FILE_EOF`
  - Direction: Server -> Client
  - Payload: File Index (4 bytes, uint32_t).
- `0x06: ERROR`
  - Direction: Server -> Client
  - Payload: Error message string.
- `0x07: TOPIC_EOF`
  - Direction: Server -> Client
  - Payload: None. Indicates all files for the topic have been transferred.

### PART II CASE 1 (Fault Injection, No Session)
Uses the exact same messages as Part I.
If a fault occurs, the TCP connection drops. The client detects the closed socket, reconnects, and issues a new `GET_REQUEST` from scratch.

### PART II CASE 2 (Session Management)
Additional messages added to support session tracking and resuming:
- `0x08: SESSION_CREATED`
  - Direction: Server -> Client
  - Payload: Session ID (8 bytes, uint64_t). Sent immediately after `TOPIC_RESPONSE` before sending files.
- `0x09: RECONNECT_REQUEST`
  - Direction: Client -> Server
  - Payload: Session ID (8 bytes), Topic (string), Last File Index (4 bytes), Last Offset (8 bytes).
- `0x0A: RESUME_RESPONSE`
  - Direction: Server -> Client
  - Payload: Status (1 byte: `0`=Rejected, `1`=Accepted). If rejected, the client must start over using `GET_REQUEST`.
- `0x0B: CHECKPOINT_ACK`
  - Direction: Client -> Server
  - Payload: File Index (4 bytes), Offset (8 bytes). Used by the client to explicitly acknowledge it has safely written data to disk (optional depending on how we track state, but useful for strict correctness).

### PART II ENHANCED (Parallel TCP Connections)
- `0x0C: WORKER_CONNECT`
  - Direction: Client -> Server
  - Payload: Session ID (8 bytes, uint64_t), Worker ID (4 bytes, uint32_t).
- `0x0D: WORK_ASSIGNMENT`
  - Direction: Server -> Client
  - Payload: File Index (4 bytes), Start Offset (8 bytes), End Offset (8 bytes).

## 3. Byte Order
All multi-byte integers (length, file index, file size, offset, session ID) MUST be transmitted in Network Byte Order (Big Endian) using `htonl`, `htons`, etc., and decoded using `ntohl`, `ntohs`, etc. For 64-bit integers, custom `htonll` and `ntohll` will be implemented.

## 4. Error Handling
- **Partial Recv/Send**: Functions `send_all` and `recv_all` must be used to loop around `send()` and `recv()` to handle partial byte transfers and `EINTR`.
- **Disconnects**: If a `recv()` returns 0 or an error occurs (`ECONNRESET`, `EPIPE`), the side detecting the failure closes its socket and triggers the failure handling mechanism (Case 1: Reconnect and restart, Case 2: Reconnect and resume).
