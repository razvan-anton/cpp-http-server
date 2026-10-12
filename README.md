# High-Concurrency HTTP Server

A lightweight, event-driven HTTP/1.1 server written in C++20 for Linux. It uses non-blocking sockets, `epoll` in edge-triggered mode, a worker thread pool, and `sendfile()` for zero-copy static file delivery.

---

## Architecture

The server uses a **Reactor pattern with a worker thread pool**:

* **Main Thread (Reactor):** Runs an `epoll_wait` loop. It accepts new TCP connections and pushes active client file descriptors into a shared task queue.
* **Worker Thread Pool:** A fixed pool of worker threads synchronized with `std::mutex` and `std::condition_variable`. Workers pop tasks from the queue and handle reading, parsing, and sending data.
* **Connection Pool:** A static array of 10,000 `ConnectionState` objects allocated up front. Each connection has a dedicated slot so there are no dynamic heap allocations (`new`/`malloc`) on incoming requests.
* **FD Recycling Protection:** Linux reuses file descriptors quickly (lowest available integer). To avoid race conditions between a worker thread exiting and the listener thread re-assigning the same FD, each connection has a mutex locked with `std::scoped_lock`. This keeps the slot locked until the worker finishes stack unwinding.

---

## Constraints

* **Linux Only:** Relies on Linux-specific syscalls (`epoll`, `sendfile`).
* **Connection Cap:** Hard limit of 10,000 concurrent connections.
* **Static Memory:** 8 KB fixed read buffer per connection (~80 MB total in `.bss`). Virtual memory means physical RAM is only used as connections actually connect.
* **Zero Per-Request Allocations:** No heap allocations on the hot path.

---

## What's Implemented

* **Edge-Triggered I/O (`EPOLLET`):** Sockets are drained in a non-blocking `while` loop until `read()` returns `EAGAIN`.
* **Worker Safety (`EPOLLONESHOT`):** Ensures only one worker thread touches a client socket at any given time.
* **Keep-Alive Support:** Re-arms sockets with `EPOLL_CTL_MOD` back to `EPOLLIN` after sending a response so clients can send multiple requests on one connection.
* **Partial Writes:** If the kernel send buffer fills up (`EAGAIN`), the socket is modified to listen for `EPOLLOUT` and resumed once writable.
* **Zero-Copy File Delivery:** Uses `sendfile()` to transfer files straight from the Linux Page Cache to the socket without copying bytes into user space.
* **Tooling & Sanitizers:** Validated race-free with ThreadSanitizer (`TSan`) and linted with Clang-Tidy (`bugprone-*`, `performance-*`, `modernize-*`).

---

## Build & Run

### Prerequisites
* Linux
* GCC / G++ with C++20 support
* `make`

### Quick Start
```bash
git clone https://github.com/razvan-anton/cpp-http-server
cd cpp-http-server
make
./my_server
```

Test1: Basic HTTP Connection
```bash
curl -i http://localhost:8080/index.html
```

Test2: ( Check image / open a browser tab at http://localhost:8080/cat.jpg )
```bash
curl -s http://localhost:8080/cat.jpg -o downloaded_cat.jpg
ls -lh downloaded_cat.jpg
```

## Benchmarks

Tested locally on Ubuntu Linux (16 cores) using `wrk` over loopback (`127.0.0.1:8080`).

### 1. Small Payload (`index.html`, 20 bytes)

```text
wrk -t16 -c100 -d10s http://127.0.0.1:8080/index.html
Requests/sec:  94,381.00
Avg Latency:   824.86us
Errors:        0

```

### 2. Zero-Copy File Transfer (`cat.jpg`, 20 KB via `sendfile`)

```text
wrk -t16 -c100 -d10s http://127.0.0.1:8080/cat.jpg
Requests/sec:  54,453.85
Transfer/sec:  10.00 GB/s (cached in RAM page cache)
Total Read:    100.43 GB in 10s

```

### 3. 5-Minute Endurance Test

```text
wrk -t16 -c100 -d5m http://127.0.0.1:8080/index.html
Requests/sec:  83,631.65
Total Req:     25,097,533 requests
Errors:        0 timeouts, 0 socket drops

```

---

## Future Improvements

* **=Timeout Reaper:** Add a doubly-linked list inside the connection array to track timestamps and evict idle/slow connections in $O(1)$ to stop Slowloris attacks.
* **Shared Buffer Pool:** Replace fixed 8 KB buffers per connection with a chunk allocator so the server can scale past 100k idle connections without wasting memory.
* **Thread-per-Core (`SO_REUSEPORT`):** Give each CPU core its own independent `epoll` loop to remove task queue mutex contention completely.
