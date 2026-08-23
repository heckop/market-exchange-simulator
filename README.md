# Market Exchange — A Low-Latency Matching Engine & Networked Exchange (C++20)

A limit-order-book **matching engine** and the **networked exchange system** around it — order-entry gateway, multi-participant fan-in, lock-free threading core, and a UDP market-data feed — built in modern C++20 and optimized by measuring at every step (~23.9M orders/sec in-process; ~1.14M orders/sec end-to-end over TCP).

## Core idea

A matching engine takes a stream of buy/sell limit orders and matches them by **price-time priority**, producing trades and a resting order book. Around that core sits a real exchange: participants send orders over the network, the engine matches them, execution reports go back to the right participant, and a market-data feed broadcasts trades and top-of-book to subscribers.

The project was built in measured phases — a cache-friendly allocation-free engine, then a single-participant network path, then multi-participant + market data, then a lock-free threading core, then syscall batching.

## Results

Single Apple Silicon Mac, macOS, Release (`-O3`), loopback networking, median of repeated runs.

**Throughput** — measured after each phase, showing the effect of every optimization:

| Stage | Change | Orders/sec |
|---|---|---|
| Engine v1 | `std::map` + `std::list`, per-order `malloc` | ~12.9M *(in-process)* |
| Engine v2 | flat tick-indexed arrays + id-indexed object pool | **~23.9M** *(in-process)* |
| + TCP, no feed | full wire protocol + gateway + matching | ~872K |
| + inline feed | market data published on the matching thread | ~188K *(56% feed loss)* |
| + Disruptor offload | lock-free ring → dedicated publisher thread | ~781K |
| + network batching | batched `send`/`sendto`, `TCP_NODELAY` | **~1.14M** *(feed loss 0%)* |

**Latency** — unloaded order round-trip (send → `Accepted`), ping-pong probe, ~99k samples:

| p50 | p99 | p99.9 |
|---|---|---|
| ~15 µs | ~30 µs | ~86 µs |

*Numbers are loopback / same-host — a floor on the mechanics, not wire time. The gap between the in-process engine (~23.9M/s) and the full networked exchange (~1.14M/s) is the residual recv/send syscall cost that kernel-bypass networking (io_uring / DPDK) would target.*

## Directory structure

```
Orderbook/
├── OrderBookTypes.cppm      # C++20 module: Price/Qty/OrderID aliases, Side & OrderType enums
├── OrderBookModels.cppm     # module: Order, OrderModify, Trade, Bbo, LevelInfo structs
├── orderBookEngine.cppm     # module: the matching engine (flat arrays + object pool)
├── test_orderbook.cpp       # GoogleTest suite (correctness for the engine)
├── main.cpp                 # small manual driver
│
├── bench/                   # in-process benchmark harness
│   ├── Timer.hpp            # monotonic ns clock
│   ├── Histogram.hpp        # latency histogram (percentiles)
│   ├── Scenario.hpp         # benchmark config (order mix, seed, price model)
│   ├── OrderGenerator.*     # realistic add/cancel/modify order stream generator
│   └── bench_main.cpp       # in-process throughput + latency benchmark
│
├── net/                     # the networked exchange
│   ├── Protocol.hpp         # fixed-size binary wire protocol + TCP FrameReader
│   ├── Gateway.*            # kqueue event loop, multi-participant, id routing, batched egress
│   ├── Journal.*            # append-only binary order/trade journal
│   ├── SpscRing.hpp         # lock-free single-producer/single-consumer ring buffer
│   ├── Feed.*               # UDP multicast market-data publisher (batched)
│   ├── exchange_main.cpp    # the exchange server (matching thread + feed publisher thread)
│   ├── participant_sim.cpp  # client: drives the exchange, validates vs in-process oracle
│   ├── feed_subscriber.cpp  # market-data subscriber (prints tape + BBO, detects gaps)
│   └── test_protocol.cpp    # unit test for the wire framing
│
└── CMakeLists.txt
```

## Tools / language / environment

- **Language:** C++20, using **modules** (`.cppm`) for the engine.
- **Build:** CMake (≥ 3.28) + **Ninja** generator (required for C++20 module dependency scanning).
- **Compiler:** **Homebrew LLVM clang** — Apple's bundled clang does not yet support C++20 modules.
- **Testing:** GoogleTest (fetched via CMake `FetchContent`).
- **Networking:** BSD sockets, `kqueue` event loop, UDP multicast, `TCP_NODELAY` (macOS/BSD APIs).
- **Machine:** Apple Silicon Mac, macOS; single machine, loopback networking. Release build (`-O3 -DNDEBUG`).

## Build, run & measure

**Configure + build** (all targets):
```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++
cmake --build build-release
```

**Correctness (engine unit tests):**
```bash
./build-release/OrderBookTests      # cross-match, fill-or-kill rejection, modify-loses-priority
```

**In-process performance** (raw engine, 5M generated orders → throughput + latency percentiles):
```bash
./build-release/OrderBookBench
```

**End-to-end networked exchange** (three terminals):
```bash
# 1: market-data subscriber (joins 239.0.0.1:9002)
./build-release/feed_subscriber
# 2: the exchange server (order entry on :9001)
./build-release/exchange
# 3: a participant — sends 5M orders, prints throughput + validates trades against an in-process oracle
./build-release/participant_sim
```
Run several `participant_sim` instances with distinct id-offsets + seeds to exercise the
multi-participant path (each prints `routing: PASS`).

## Key features

- **Allocation-free matching engine**: flat tick-indexed price arrays (O(1) level access, no `std::map`), an id-indexed object pool (zero hot-path `malloc`), and intrusive index-based doubly-linked FIFO lists (O(1) cancel, reallocation-safe). ~23.9M orders/sec in-process.
- **Full order semantics**: price-time priority; GoodTillCancel, FillOrKill, FillAndKill, Market, GoodForDay; modify with correct priority rules.
- **Binary wire protocol + TCP framing**: fixed-layout messages, length-framed, with correct partial-read / coalesced-message handling over the byte stream.
- **Multi-participant gateway** on a single `kqueue` event loop: per-connection framing, `client_oid ↔ internal_id` translation, and two-sided fill routing to the correct counterparties.
- **Lock-free LMAX-style SPSC ring buffer**: acquire/release atomics + cache-line padding; decouples market-data publishing from matching so the feed can never stall order flow.
- **UDP multicast market-data feed**: trade prints + top-of-book, with per-message sequence numbers so drops are detectable.
- **Syscall batching**: exec reports coalesced per recv-batch; feed events packed many-per-datagram; `TCP_NODELAY`. Lifted networked throughput 781K → 1.14M and eliminated feed loss (56% → 0%).
- **Measured, staged optimization**: every change validated by a benchmark and a correctness framework (network trade sequence checked byte-for-byte against a direct in-process replay).

## Extended features (possible next steps)

- **Kernel-bypass networking** — `io_uring` (fits the current TCP design; batches/asyncs syscalls) or **DPDK** (userspace NIC, needs real hardware) to close the ~20× gap between networked and in-process throughput.
- **Clean unloaded latency percentiles** — a ping-pong probe for true round-trip p50/p99/p99.9 (current RTT numbers are loaded/queuing latency under full blast).
- **Full LMAX Disruptor separation** — ingress / matching / egress as separate ring-connected stages.
- **Full incremental L2 market data** (order-by-order add/execute/cancel deltas, ITCH-style) with feed snapshot/recovery.
- **Linux `epoll` port** for cross-platform networking; **`cntvct`/`rdtsc` timing** for sub-clock-resolution latency.
- **Persistence/recovery** — replay from the journal to rebuild book state after restart.
```
