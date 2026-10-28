# cpp-trading-exchange

A high-performance trading exchange engine written in C++26.

The exchange implements core matching-engine components: order intake, a price-time-priority matching algorithm, and market-data dissemination — built with low-latency primitives (lock-free queues, memory pools, RDTSC timing).

## Quick start

```bash
bazel build //...   # build everything
bazel test //...    # run all tests
```

See [BAZEL_BUILD.md](BAZEL_BUILD.md) for full build instructions, available targets, and compiler flags.

## Project layout

```
common/      # shared low-latency utilities (lock-free queue, memory pool, logging)
exchange/
  matcher/        # price-time-priority matching engine
  order_server/   # order intake and session management
  market_data/    # market-data feed handler
examples/    # standalone benchmarks and demos
```

## Setup notes (Workshop 1)

Verified on macOS 26.3 / Apple M4 Pro with Apple clang 17 (Xcode 26.2) and Bazel 9.3.0 (Homebrew).

```bash
bazel build //...                                        # build everything
bazel test //...                                         # 2 tests pass
bazel run --config=opt //examples:container_benchmark    # vector vs map vs unordered_map
```

### container_benchmark results on my machine

Units are ticks of the 24 MHz ARM64 system counter (1 tick ≈ 41.7 ns), see `common/perf_utils.h`.

| N = 10,000,000        |     vector |         map | unordered_map |
|-----------------------|-----------:|------------:|--------------:|
| Insert (total)        |  6,835,771 | 388,347,199 |    93,429,824 |
| Lookup (avg/op)       |        178 |         278 |            16 |
| Lookup cold (avg/op)  |      1,876 |       2,637 |            14 |
| Iterate (total)       | 10,381,976 |  26,962,480 |    10,714,211 |

Takeaways: unordered_map wins every lookup; vector wins insert and iterate; map is slowest but is the only one that keeps keys sorted.


## Workshops 2–3: memory pool and order book

- `common/mem_pool.h` — `MemPool<T>`: fixed-capacity object pool. One allocation up front; `allocate()` / `deallocate()` never touch the heap and construct / destroy objects in place (`std::construct_at` / `std::destroy_at`). Free flags live in a bit-packed `std::vector<bool>` separate from the object slots (structure of arrays), so the free-slot scan only touches the flags.
- `exchange/matcher/order.h` — `Order`: plain data plus `toString()`. Prices are integer ticks (1 tick = $0.01).
- `exchange/matcher/order_book.h` — `OrderBook` with `add()`, `cancel()`, `toString()`. Orders live in a `MemPool<Order>`; `std::unordered_map` gives O(1) cancel by id; `std::map` keeps bid / ask levels sorted with the best price at `begin()`; orders at one price form an intrusive FIFO list through `Order::prev` / `Order::next`.

```bash
bazel test //...                            # 5 tests
bazel run //examples:order_book_example     # add / cancel walkthrough
```