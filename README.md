# NeuroChat

NeuroChat is a multiclient chat server written in C++ for Ubuntu/Linux sockets and POSIX threading.

Instead of creating one thread per client, the server uses a fixed thread pool, synchronized shared state, and connection limiting. It is designed as an OS course project that demonstrates concurrency concepts in a practical chat system.

## Implemented Features

### Core concurrency features
- Fixed-size thread pool for client handling.
- Shared queue for accepted client sockets.
- Mutex-protected shared client list and routing operations.
- Semaphore-based cap on active connections.

### Chat functionality
- Authentication with username and password.
- Registration support with persisted users in users.txt.
- Room-based chat via /join room_name.
- Private messaging via /msg username your_message.
- Basic anti-spam delay (rate limiting).

### Observability and monitoring
- Timestamped server logging in server_log.txt.
- GUI monitor panel receives live server state packets:
  - active worker count
  - connection/slot usage
  - mutex lock state indicator
  - total message counter

## Project Structure

- server.cpp: multithreaded chat server.
- client.cpp: terminal client.
- client_gui.cpp: GTK client with monitor panel.
- Makefile: Linux build targets.
- server_log.txt: server runtime logs.
- users.txt: persisted registered users (generated when registration is used).

## Build and Run (Ubuntu/Linux)

### 1) Build

```bash
make
```

Or build individual targets:

```bash
make server
make client
make client_gui
```

### 2) Start server

```bash
./server
```

### 3) Start terminal client(s)

```bash
./client
```

### 4) Start GUI client (optional)

```bash
./client_gui
```

## Login and Registration Flow

When a client connects, the server prompts:
- Mode (login/register)
- Username
- Password

Examples:
- login -> use existing user
- register -> create a new user and persist to users.txt

Default built-in users include:
- admin / 1234
- badar / 24k3079
- shayan / 24k5613
- aarib / k243100

## Commands

- /join room_name: switch current room.
- /msg username message: private message.
- /monitor on: enable monitor stream for that client.
- /monitor off: disable monitor stream.
- exit (terminal client): disconnect.

## Notes for Windows Users

This code uses POSIX/Linux headers and APIs (pthread, sys/socket.h, arpa/inet.h, semaphore).
Run it in:
- Ubuntu/Linux directly, or
- WSL on Windows with required build dependencies.

## How This Differs from a Simple Client-Server Task

A basic client-server socket assignment usually does this:
- one server thread/process with minimal request loop
- no authentication lifecycle
- no rooms/private routing
- no load control
- no synchronization stress with shared state
- little or no observability/logging

NeuroChat goes further by adding:
- thread pool architecture instead of per-client thread creation
- synchronized shared state with mutexes under concurrent access
- semaphore-based admission control for overload protection
- multi-feature protocol (auth/register, rooms, private messaging, anti-spam)
- runtime monitoring and timestamped logging for debugging and demo visibility

In short: this project is not just "send/receive over TCP". It is a concurrent, stateful chat backend with OS-level synchronization behavior visible during execution.
