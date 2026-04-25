# NeuroChat — High-Performance OS-Level Chat Server (C++)

A multi-client chat server built in **C++ on Linux**, designed to simulate how real-world systems handle **concurrent users efficiently** using **thread pools, mutexes, and semaphores**.

This project focuses on **performance, synchronization, and scalability**, turning core Operating System concepts into a working, real-time system.

---

## Why This Project Matters

Most beginner chat apps create a **new thread per client** — that’s inefficient and breaks under load.

**NeuroChat solves this by:**
- Using a **thread pool** (constant memory usage)
- Ensuring **safe concurrency** (no data corruption)
- Limiting overload using **semaphores**
- Handling **multiple users simultaneously without crashes**

This is closer to how **real backend servers** are built.

---

## ⚙️ Core Features

### Thread Pool Architecture
- Fixed number of worker threads
- Reuses threads instead of creating/destroying per client
- Improves **performance + scalability**

### Mutex-Based Synchronization
- Shared message buffer protected with mutex locks
- Prevents race conditions and message corruption

### Semaphore-Based Load Control
- Controls max active clients
- Prevents server overload under heavy traffic

### Concurrent Client Handling
- Each client handled by an available thread
- Supports multiple users in real time

---

## Advanced Features

### Authentication System
- Username/password-based login
- Simulates real-world access control

### Private Messaging
- Send direct messages using:


### Multiple Chat Rooms
- Users can join rooms like:
- general
- gaming
- study
- Messages scoped per room

### Server Logging
- Logs:
- connections
- disconnections
- messages
- Useful for debugging and monitoring

### Rate Limiting
- Prevents spam
- Slows down or blocks excessive requests


## System Architecture
- Client (GUI / Terminal)
↓
- TCP Socket Layer
↓
- Thread Pool (Worker Threads)
↓
- Shared Message Buffer (Mutex Protected)
↓
- Routing Logic (Rooms / Private Messages)
↓
- Broadcast / Direct Delivery


---

## OS Concepts Demonstrated

This isn’t just a chat app — it’s a **live OS lab**:

- Thread lifecycle management  
- Race condition prevention  
- Deadlock avoidance (via careful locking)  
- Producer-consumer style buffering  
- Resource limiting using semaphores  

---

## GUI + Live OS Monitor

A unique part of this project:

- Real-time chat interface  
- Live panel showing:
  - Active threads
  - Mutex lock state
  - Semaphore usage  
 Makes invisible OS concepts **visually understandable**

---

## 🛠️ Tech Stack

- **Language:** C++  
- **OS:** Ubuntu Linux  
- **Threading:** POSIX Threads (pthreads)  
- **Networking:** TCP Sockets (Linux Socket API)  
- **GUI:** Qt / GTK  
- **Build Tools:** g++, Makefile  
- **Version Control:** GitHub  

---

## 🚀 Getting Started

### 1. Clone Repository
    ```bash git clone https://github.com/your-username/neurochat.git cd neurochat
### 2. Build
    ```bash
    make
### 3. Run Server
    ```bash
    ./server
### 4. Run Client (multiple terminals)
     ```bash
     ./client

## Performance Highlights
- Handles multiple concurrent clients efficiently
- Constant thread count → predictable memory usage
- No message corruption under concurrent load
- Graceful handling of overload scenarios     
  
