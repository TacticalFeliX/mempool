# MemPool - Fixed-Size Memory Pool Allocator

A lightweight, high-performance, **$O(1)$ constant-time** memory pool allocator written in C99. Designed specifically for resource-constrained systems, embedded targets, and real-time environments where dynamic heap allocation (`malloc`/`free`) is forbidden or risks memory fragmentation.

This project implements an **intrusive free list** trick: when a memory block is free, its raw bytes are repurposed to hold a structure pointing to the next free block. No extra bookkeeping overhead or heap structures required.

---

## 🚀 Key Features

* **Deterministic Performance:** Both allocation (`pool_alloc`) and deallocation (`pool_free`) operate in strict $O(1)$ time complexity.
* **Zero Fragmentation:** Fixed-size chunk architecture completely eliminates external memory fragmentation.
* **No Heap Footprint:** Utilizes a statically sized flat array allocated on the stack, data section, or BSS segment. Ideal for bare-metal embedded targets.
* **Intrusive Free List:** Reuses free slots to embed list pointers, keeping metadata overhead entirely within unused memory.
* **Compile-time Safety:** Built-in static assertions verify that your configurations don't lead to undefined behavior or misaligned structures.
* **Diagnostic Reporting:** Includes built-in tracking counters (`blocks_in_use`, `total_allocs`, `total_frees`) to generate real-time health reports.

---

## 🛠️ Architecture Overview

The allocator partitions a single flat byte array (`pool->storage[]`) into `POOL_BLOCK_COUNT` fixed-size slots, each containing `POOL_BLOCK_SIZE` bytes.

💻 Quick Start & Usage
Building the Demo
To compile the test driver, run:

```bash
gcc -Wall -Wextra -std=c11 -o mempool_demo main.c mempool.c
```

📊 Sample Metrics Report
Calling pool_print_stats() renders an ASCII visualizer summarizing lifetime and active telemetry:

Plaintext
┌──────────────────────────────────────────┐
│          MemPool Status Report           │
├──────────────────────────────────────────┤
│  Block size   :   64 bytes               │
│  Total blocks :   16                     │
│  In use       :    3                     │
│  Free         :   13                     │
│  Total allocs :    4 (lifetime)          │
│  Total frees  :    1 (lifetime)          │
└──────────────────────────────────────────┘
