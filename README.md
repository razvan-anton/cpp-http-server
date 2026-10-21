# High-Concurrency HTTP Server

A lightweight, event-driven HTTP/1.1 server written in C++20 for Linux. It uses non-blocking sockets, `epoll` in edge-triggered mode, a worker thread pool, and `sendfile()` for zero-copy static file delivery.

---

## Architecture

The server uses a **Reactor pattern with a worker thread pool**:

- **Main Thread (Reactor):** Runs an `epoll_wait` loop. It accepts new TCP connections and pushes active client file descriptors into a shared task queue.
- **Worker Thread Pool:** A fixed pool of worker threads synchronized with `std::mutex` and `std::condition_variable`. Workers pop tasks from the queue and handle reading, parsing, and sending data.
- **Connection Pool:** A static array of 10,000 `ConnectionState` objects allocated up front. Each connection has a dedicated slot so there are no dynamic heap allocations (`new`/`malloc`) on incoming requests.
- **FD Recycling Protection:** Linux reuses file descriptors quickly (lowest available integer). To avoid race conditions between a worker thread exiting and the listener thread re-assigning the same FD, each connection has a mutex locked with `std::scoped_lock`. This keeps the slot locked until the worker finishes stack unwinding.

---

## Constraints

- **Linux Only:** Relies on Linux-specific syscalls (`epoll`, `sendfile`).
- **Connection Cap:** Hard limit of 10,000 concurrent connections.
- **Static Memory:** 8 KB fixed read buffer per connection (~80 MB total in `.bss`). Virtual memory means physical RAM is only used as connections actually connect.
- **Near-Zero Per-Request Allocations:** No heap allocations on the hot path.

---

## What's Implemented

- **Edge-Triggered I/O (`EPOLLET`):** Sockets are drained in a non-blocking `while` loop until `read()` returns `EAGAIN`.
- **Worker Safety (`EPOLLONESHOT`):** Ensures only one worker thread touches a client socket at any given time.
- **HTTP/1.1 Keep-Alive & Pipelining:** Reuses persistent connections and shifts remaining bytes in the buffer to handle back-to-back pipelined requests without dropping data.
- **Startup Path Cache:** Pre-indexes `./public` into a static hash map on startup and returns `const std::string*` pointers, avoiding `std::filesystem::canonical` syscalls and heap allocations on the hot path.
- **Partial Writes:** If the kernel send buffer fills up (`EAGAIN`), the socket is modified to listen for `EPOLLOUT` and resumed once writable.
- **Zero-Copy File Delivery:** Uses `sendfile()` to transfer files straight from the Linux Page Cache to the socket without copying bytes into user space.
- **Tooling & Sanitizers:** Validated race-free with ThreadSanitizer (`make tsan`), profiled with Tracy (`make profile`), and scanned with Clang-Tidy.

---

## Build & Run

### Prerequisites
- Linux
- GCC / G++ with C++20 support
- `make`

### Quick Start
```bash
git clone https://github.com/razvan-anton/cpp-http-server
cd cpp-http-server
make
./my_server

```

**Other Build Targets:**

```bash
make tsan     # Build with ThreadSanitizer (-fsanitize=thread -O1 -g)
make profile  # Build with Tracy Profiler instrumentation

```

**Test 1: Basic HTTP Connection**

```bash
curl -i http://localhost:8080/index.html

```

**Test 2: Download Static File**

```bash
curl -s http://localhost:8080/cat.jpg -o downloaded_cat.jpg
ls -lh downloaded_cat.jpg

```

---

## Benchmarks & Optimizations

Tested locally on Ubuntu Linux (16 logical cores) using `wrk` (`wrk -t16 -c100 -d10s --latency`) over loopback (`127.0.0.1:8080`).

The server was optimized across three steps:

1. **Baseline (Teardown):** Close connection after every request.
2. **HTTP/1.1 Keep-Alive:** Instead of closing every upcoming connections, support for both keep-alive and close was implemented, according to the HTTP protocol.
3. **Startup Path Cache:** Replaced per-request `std::filesystem::canonical` calls with a startup-initialized hash table (`std::unordered_map`) returning `const std::string*` pointers.

### 1. Small Payload (`index.html`)

| Metric | Baseline (Teardown) | Keep-Alive + `TCP_NODELAY` | Startup Path Cache | Total Gain |
| --- | --- | --- | --- | --- |
| **Throughput** | 94,381 req/sec | 240,286 req/sec | **364,218 req/sec** | **+285.9% (3.86x)** |
| **Transfer Rate** | 9.27 MB/s | 172.32 MB/s | **261.20 MB/s** | **28.2x** |
| **Avg Latency** | 824.86 µs | 412.10 µs | **358.39 µs** | **-56.6%** |
| **Median (p50)** | 910.00 µs | 330.00 µs | **201.00 µs** | **-77.9%** |
| **p90 Latency** | 1.33 ms | 1.14 ms | **802.00 µs** | **-39.7%** |
| **p99 Latency** | 7.59 ms | 3.01 ms | **2.63 ms** | **-65.3%** |

```text
$ wrk -t16 -c100 -d10s --latency http://127.0.0.1:8080/index.html
  3647184 requests in 10.01s, 2.55GB read
Requests/sec: 364218.36
Transfer/sec:    261.20MB

```

### 2. Zero-Copy File Transfer (`cat.jpg`, ~180 KB via `sendfile`)

| Metric | Baseline (Teardown) | Keep-Alive + `TCP_NODELAY` | Startup Path Cache | Total Gain |
| --- | --- | --- | --- | --- |
| **Throughput** | 54,453 req/sec | 93,714 req/sec | **95,392 req/sec** | **+75.2% (1.75x)** |
| **Transfer Rate** | 10.00 GB/s | 17.20 GB/s | **17.51 GB/s** | **+7.51 GB/s** |
| **Total Read (10s)** | 100.43 GB | 172.46 GB | **175.60 GB** | **+75.17 GB** |
| **Median (p50)** | 1.60 ms | 710.00 µs | **654.00 µs** | **-59.1%** |
| **p90 Latency** | 3.02 ms | 1.45 ms | **1.46 ms** | **-51.7%** |
| **p99 Latency** | 5.02 ms | 2.55 ms | **2.79 ms** | **-44.4%** |

```text
$ wrk -t16 -c100 -d10s --latency http://127.0.0.1:8080/cat.jpg
  956568 requests in 10.03s, 175.60GB read
Requests/sec:  95392.65
Transfer/sec:     17.51GB

```

### 3. Profiling with Tracy (`get_abs_path` Bottleneck)

Profiling with Tracy showed that `get_abs_path` was taking **24.59 µs per call** (27.74s total across 1.1M requests) because `std::filesystem::canonical` was hitting the kernel VFS layer on every request.

After caching `./public` paths at startup, `get_abs_path` dropped out of the hot path completely, pushing throughput from **240k to 364k RPS**. Also verified zero mutex contention across worker threads.

### Before Path Cache: ( around 25 microseconds per call ):

<img width="929" height="454" alt="image" src="https://github.com/user-attachments/assets/9099495e-b984-438e-aaa1-05f82096437e" />



### After Path Cache: Out of the hot path ( around 25 nanoseconds per call ):

<img width="1282" height="502" alt="image" src="https://github.com/user-attachments/assets/0c003ba1-57b7-4b4f-ba60-279200069935" />



---

## Future Improvements

* **Timeout Reaper:** Add a doubly-linked list inside the connection array to track timestamps and evict idle/slow connections in $O(1)$ to stop Slowloris attacks.
* **Shared Buffer Pool:** Replace fixed 8 KB buffers per connection with a chunk allocator so the server can scale past 100k idle connections without wasting memory.
* **Thread-per-Core (`SO_REUSEPORT`):** Give each CPU core its own independent `epoll` loop to remove task queue mutex contention completely.

