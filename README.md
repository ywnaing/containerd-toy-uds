# 🐳 containerd-toy-uds: gRPC over Unix Domain Socket in C

A lightweight, bare-metal implementation of local IPC using **Unix Domain Sockets (`AF_UNIX`)** and the **gRPC 5-byte length-prefixed wire framing protocol**, mimicking the communication mechanism between container runtimes (like `containerd.sock`) and orchestrators (like Kubernetes `kubelet` or `crictl`).

---

## 🏗️ Architecture

```
[ Client (crictl / CLI) ]
         │
         ▼  (gRPC length-prefixed frames: [1B flag] + [4B length] + [payload])
┌────────────────────────────────────────────────────────┐
│ Transport: Unix Domain Socket (/tmp/containerd_toy.sock)│
├────────────────────────────────────────────────────────┤
│ Auth: Kernel Peer Credentials (UID / GID / PID)        │
└────────────────────────────────────────────────────────┘
         │
         ▼
[ Daemon (containerd_toy server) ]
```

---

## 🚀 Key Features

1. **`AF_UNIX` Stream IPC**: Bypasses the TCP/IP network stack entirely for direct kernel memory buffer transfers.
2. **Kernel Peer Credentials**: The daemon queries the kernel via `getpeereid()` and `LOCAL_PEERPID` to securely authenticate the caller's process ID (`PID`), user ID (`UID`), and group ID (`GID`) without passwords.
3. **gRPC Wire Protocol Framing**: Implements the official gRPC 5-byte frame format (`[1B compressed flag] + [4B big-endian length]`) to handle stream fragmentation and message boundaries.
4. **RPC Method Dispatch**: Supports CRI/containerd container lifecycle commands (`RunContainer`, `ListContainers`, `StopContainer`) over a persistent connection stream.

---

## 🛠️ Build & Run

### 1. Compile
```bash
make
```

### 2. Start the Daemon (Terminal 1)
```bash
./server
```

### 3. Run the Client (Terminal 2)
```bash
./client
```
