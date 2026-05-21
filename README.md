# Multiplexed Telemetry Stream — `unified_solution.cpp`

A single-file C++17 demo that models a **multiplexed sensor bus**: multiple hardware processors share one polymorphic interface, each ingesting telemetry frames into its own ring buffer and stamping frames with a device identity.

This document focuses on [`unified_solution.cpp`](unified_solution.cpp) — everything in one translation unit for easier reading and review.

**There is also an equivalent split-file version** of the same multiplexed telemetry design:

| File | Role |
|------|------|
| [`TelemetryBuffer.hpp`](TelemetryBuffer.hpp) | Declarations: `TelemetryFrame`, `BaseProcessor`, `CoreProcessor`, `OscilloscopeProcessor` |
| [`TelemetryBuffer.cpp`](TelemetryBuffer.cpp) | Implementations + `extern "C"` factory API |
| [`main.cpp`](main.cpp) | Entry point: builds the bus, runs `DataContract`, audits connection status |

The split layout matches typical production C++ (header vs implementation, separate `main`). Processor logic (`DataContract`, ring buffers, stamping) is the same in spirit; **`main.cpp` does not include the unified file’s dynamic bus orchestration.**

### Split `main.cpp` vs unified `main` — bus and ingest

| Behavior | Split (`main.cpp`) | Unified (`unified_solution.cpp`) |
|----------|-------------------|----------------------------------|
| Devices on the bus | Two **named** pipelines (`fpgaPipeline`, `scopePipeline`), two manual `push_back` calls | Same two-device setup, plus `labBenchNetwork.reserve(10)` and comments for adding more slots |
| Scaling to **N** devices | **Fixed at two** — no loop that allocates/registers hardware from config or discovery | Ingest **scales with vector size** via range-`for`; easy to add more `push_back`s without changing ingest code |
| Ingest / routing | **Hardcoded indices:** `labBenchNetwork[0]` + `sampleA`, `labBenchNetwork[1]` + `sampleB` inside `if (size >= 2)` | **Dynamic loop:** `for (const auto& device : labBenchNetwork)` builds a fresh packet per device and calls `device->DataContract(...)` |
| Connection audit | Runs `GetConnectionStatus` over all bus indices | Not included (ingest demo only) |

So the split-file version still uses a `vector<unique_ptr<BaseProcessor>>` as the multiplex bus, but it does **not** dynamically allocate/register an arbitrary number of devices or drive ingest from a single loop over **N** bus entries. It wires exactly two processors and two frames by index.

The unified file is where the **N-device pattern** appears: reserve space on the bus, optionally append more processors, then let one ingest loop walk every element without knowing concrete types at each index.

**Note:** [`unified_solution.cpp`](unified_solution.cpp) is the complete, buildable reference. The split files mirror processor design; — use the unified file if you want a one-command build with dynamic allocation for N number of devices (uses 10 as an example number of devices, but would be dynamically allocaed value at run-time based on a device scan in a production scenerio).

### Build and run (unified)

```bash
c++ -std=c++17 -Wall -Wextra -o unified_solution unified_solution.cpp
./unified_solution
```

### Build and run (split files)

```bash
c++ -std=c++17 -Wall -Wextra -o multiplexed_stream main.cpp TelemetryBuffer.cpp
./multiplexed_stream
```

Expected output (order may vary at shutdown):

- FPGA and oscilloscope ingest logs with frame IDs and enum values `1` and `2`
- Destructor messages when `main` exits

---

## High-level architecture

```text
main
  │
  ├─ vector<unique_ptr<BaseProcessor>>  labBenchNetwork   ("multiplex bus")
  │
  ├─ [0] CoreProcessor          ── DataContract ──► ring buffer (capacity 5)
  └─ [1] OscilloscopeProcessor  ── DataContract ──► ring buffer (capacity 5)

extern "C" API (optional FFI path)
  CreateCoreProcessor → void*
  IngestTelemetryFrame → DataContract
  DestroyProcessor → delete via virtual ~BaseProcessor
```

**Idea:** Callers hold `BaseProcessor*` (or `unique_ptr<BaseProcessor>`). Virtual `DataContract` dispatches to the correct child. Each child owns a fixed-size circular `memory_pool` of `TelemetryFrame` objects on the heap.

---

## File layout (top to bottom)

| Section | What it contains |
|--------|-------------------|
| Includes + macros | Standard library headers; `MAX_VOLTAGE`, `SYS_PORT` placeholders |
| `DeviceType` | Scoped enum: `UNKNOWN`, `FPGA_TRACKER`, `OSCILLOSCOPE` |
| `TelemetryFrame` | Cache-aligned telemetry packet struct |
| `BaseProcessor` | Abstract base: serial, mutex, virtual `DataContract` |
| `CoreProcessor` | FPGA implementation + ring buffer |
| `OscilloscopeProcessor` | Scope implementation + ring buffer |
| `extern "C"` | C-callable factory/ingest/destroy (e.g. Unity / FFI) |
| `main` | Builds bus, runs dynamic ingest loop |

There is no `#pragma once` — this file is compiled once directly, not included elsewhere.

---

## Core types

### `TelemetryFrame`

```cpp
struct alignas(64) TelemetryFrame {
    uint32_t frame_id;
    uint64_t sys_time_res;
    float sensor_measurements;
    bool is_validated;
    DeviceType source_device;
};
```

- **`alignas(64)`** — pads/aligns the struct (cache-line style layout; useful in sensor/embedded style designs).
- **`source_device`** — set to `UNKNOWN` before ingest; each processor **stamps** it after storing the frame (`FPGA_TRACKER` or `OSCILLOSCOPE`).

### `BaseProcessor`

- **`serial_n`** — device serial string (moved in via constructor).
- **`m_0`** — `mutex` used with `lock_guard` in `DataContract` / `GetConnectionStatus` (**example / interview pattern only** — see [Mutex / lock_guard (teaching only)](#mutex--lock_guard-teaching-only)).
- **`virtual ~BaseProcessor() = default`** — required for safe `delete` through base pointers (C API and `unique_ptr<BaseProcessor>`).
- **`virtual void DataContract(TelemetryFrame&&)`** — pure virtual ingest contract.

### `CoreProcessor` / `OscilloscopeProcessor`

Both children share the same private shape:

| Member | Role |
|--------|------|
| `memory_pool` | `vector<unique_ptr<TelemetryFrame>>` — ring buffer slots |
| `circular_buffer_index` | `atomic<size_t>` — next write index (lock-free bump) |
| `m_capacity` | Ring size (e.g. `5` in `main`) |

**Constructor:** `memory_pool.resize(capacity)`, `circular_buffer_index.store(0, relaxed)`.

**`DataContract` (same steps in both children):**

1. `lock_guard` on `m_0`
2. `targetIndex = fetch_add(1, relaxed) % m_capacity`
3. `memory_pool[targetIndex] = make_unique<TelemetryFrame>(move(frame))`
4. Stamp `source_device` with the child’s enum
5. Log confirmation

Older frames in a slot are destroyed automatically when the `unique_ptr` is overwritten.

---

## Architectural choices (interview narrative)

Two deliberate design decisions in [`unified_solution.cpp`](unified_solution.cpp) `main()` are worth calling out on a whiteboard or in a system-design discussion. Line numbers below refer to that file.

### 1. Decoupled interface + polymorphic bus (heterogeneous **N** devices)

**Where in code (lines 150–159, 168–172):**

```cpp
std::unique_ptr<BaseProcessor> fpgaPipeline =
    std::make_unique<CoreProcessor>("FPGA-CORE-01", 5);

std::unique_ptr<BaseProcessor> scopePipeline =
    std::make_unique<OscilloscopeProcessor>("SCOPE-HW-02", 5);

labBenchNetwork.push_back(std::move(fpgaPipeline));
labBenchNetwork.push_back(std::move(scopePipeline));

for (const auto& device : labBenchNetwork) {
    device->DataContract(std::move(freshPacket));
}
```

**Explenation:**

We **explicitly instantiate** hardware through **decoupled interface syntax**: the tracking variable is typed as the abstract parent (`unique_ptr<BaseProcessor>`), while `make_unique` allocates the **concrete** child (`CoreProcessor`, `OscilloscopeProcessor`). The left-hand type is the universal socket; the right-hand side is the specific engine.

Because those handles are expressed at the **base abstraction layer**, they are **interchangeable** at the orchestration level: the orchestrator does not depend on FPGA vs scope concrete types when building the bus. We transfer **exclusive ownership** into `labBenchNetwork` with `std::move` on each `unique_ptr` (no copy of the processor object; the local pipeline variables become null after `push_back`).

The result is a `vector` that can hold an **arbitrary N** of **heterogeneous** instruments side by side—each slot is `unique_ptr<BaseProcessor>` but may point at different child types. The master loop walks that bus **without knowing** which chip sits at each index: it passes `TelemetryFrame` as an **rvalue** (`std::move`), and the compiler dispatches `DataContract` through the **vtable** to the correct child rules (`override` / `final` on each processor).

That yields a sensor-fusion style layout where **user-facing ingest loops stay the same** as N grows: add another `push_back(std::make_unique<...>(...))` (or a factory loop); you do **not** rewrite the range-`for` ingest loop for each new device type.

**Caveat for accuracy:** This demo still constructs two devices explicitly; the **pattern** scales to N, not magic runtime discovery of types by itself.

---

### 2. `reserve(n)` — deterministic vector growth (production init)

**Where in code (lines 142–146):**

```cpp
std::vector<std::unique_ptr<BaseProcessor>> labBenchNetwork;
labBenchNetwork.reserve(10);  // stand-in for runtime n
```

**Explenation:**

In production we avoid **non-deterministic vector allocation jitter** by sizing the bus **before** insertion. During initialization, the backend either runs a **hardware discovery scan** or parses **system configuration** to obtain the total instrument count **n**. We pass that runtime value into `labBenchNetwork.reserve(n)` **before** any `push_back`.

`reserve(n)` does not create n elements; it allocates capacity for **up to n** upcoming `push_back` operations without reallocation. That gives a **contiguous backing array** sized for the expected device footprint so that, as we populate the polymorphic bus, the vector is unlikely to **reallocate and move** stored `unique_ptr`s mid-registration—helping keep registration-time behavior **flat and predictable** (important for soft-real-time / telemetry pipelines).

In this repo, `10` is a **placeholder** for that runtime **n** (with only two devices pushed today). The comment in source notes that a real deployment would set `n` from discovery/config, not a literal constant.

**Caveat for accuracy:** `reserve` optimizes **capacity** growth; it does not alone make ingest lock-free or real-time—that is a separate concern (see [Mutex / lock_guard (teaching only)](#mutex--lock_guard-teaching-only)).

---

## `main()` flow

1. **Create bus** — `vector<unique_ptr<BaseProcessor>> labBenchNetwork` (line 142).
2. **Reserve capacity** — `reserve(10)` as stand-in for runtime **n** (line 146).
3. **Decoupled instantiation** — `unique_ptr<BaseProcessor>` ← `make_unique<CoreProcessor>` / `OscilloscopeProcessor` (lines 150–154).
4. **Transfer ownership** — `push_back(std::move(...))` onto the bus (lines 158–159).
5. **Dynamic ingest loop** — range-`for`; `device->DataContract(std::move(freshPacket))` via vtable (lines 168–172).
6. **Shutdown** — vector destructors run child then base destructors (RAII).

The ingest loop scales to **N** bus entries without hardcoding `[0]` / `[1]` indices.

---

## Key C++ mechanisms

| Mechanism | Where used |
|-----------|------------|
| **Polymorphism** | `unique_ptr<BaseProcessor>` + `virtual DataContract` |
| **`override` / `final`** | Child `DataContract` implementations |
| **`std::move`** | Serial string, frames into pool, pipelines into vector |
| **`std::make_unique`** | Heap frames and processors with clear ownership |
| **`std::lock_guard`** | RAII mutex lock in `DataContract` / `GetConnectionStatus` (teaching example) |
| **`std::atomic` + `memory_order_relaxed`** | Lock-free index bump for the ring-buffer write index |
| **Ring buffer** | `fetch_add % m_capacity` + overwrite slot in `memory_pool` |

---

## Mutex / `lock_guard` (teaching only)

`BaseProcessor` owns `mutable std::mutex m_0`, and both `DataContract` and `GetConnectionStatus` acquire it with `std::lock_guard<std::mutex>`.

**Why it is in this repo:** Mutexes and RAII locking (`lock_guard`) are common in C++ interviews and course material. This project includes them as a **minimal, readable example** of “guard shared state while you mutate it.”

**Why you would remove them in production:** Here, every ingest holds one mutex for the whole `DataContract` body. Under real load (high frame rate, many devices, multiple threads), that becomes a **serialization bottleneck** — threads queue on the same lock even when they could write to different slots or different processors independently.

A production-oriented design would more likely:

- Rely on **per-device ownership** (one ingest thread per processor, or single-threaded ingest per device), and/or
- Use **lock-free or finer-grained** synchronization (e.g. atomics for the index only, SPSC/MPSC queues, double buffering), and/or
- Avoid locking around logging and heap allocation on the hot path

So: **keep the mutex in this exercise to discuss RAII and thread-safety; plan to drop or narrow it in a real system** where throughput matters. The `atomic` index bump is the hint toward the lock-free direction; the mutex is not presented as the final production pattern.

---

## `extern "C"` API

For callers that cannot use C++ types directly (e.g. game engines):

| Function | Behavior |
|----------|----------|
| `CreateCoreProcessor(serial, capacity)` | `new CoreProcessor` → `void*` |
| `IngestTelemetryFrame(handle, id, time, measurement, valid)` | Builds stack `TelemetryFrame`, calls `DataContract` through `BaseProcessor*` |
| `DestroyProcessor(handle)` | `delete` through base pointer (virtual destructor runs child cleanup) |

**Note:** `CreateCoreProcessor` only exposes the FPGA type; a full production API would add factories for other device types.

---

## Design choices vs split files

| Split project | `unified_solution.cpp` |
|---------------|-------------------------|
| Declarations in `.hpp`, definitions in `.cpp` | Methods defined **inside** classes |
| `~BaseProcessor() {}` in `.cpp` | `virtual ~BaseProcessor() = default` inline |
| `#pragma once` on header | Not needed (single translation unit) |

Behavior matches the multi-file version; structure is simplified for reading and interview review.

---

## Constants and parameters

- **`5` (capacity)** — Number of ring-buffer slots per processor (exercise default, not a hardware constant).
- **`DeviceType` values in logs** — `1` = `FPGA_TRACKER`, `2` = `OSCILLOSCOPE` (printed via `static_cast<int>` because `enum class` does not stream directly).

---

## Possible extensions

- **Config-driven bus** — `DeviceConfig` + factory loop instead of named `fpgaPipeline` / `scopePipeline`.
- **Frame-based routing** — add `route_to` on `TelemetryFrame` and dispatch by metadata instead of “one packet per bus slot.”
- **Read path** — iterate `memory_pool` and stream frames to fusion/logging (ingest-only today).
- **N devices** — extra `push_back(make_unique<...>)` entries; ingest loop already iterates all nodes.

---

## Related files

| File | Role |
|------|------|
| **`TelemetryBuffer.hpp` + `TelemetryBuffer.cpp` + `main.cpp`** | **Primary split-file solution** (same project, multi-file layout) |
| `main_unfinished.cpp` | Exercise starter template |
| `solution.cpp` | Alternate / partial solutions |
| `scratch.cpp` | Practice / experiments |

---

## Quick reference: one ingest call

```text
device->DataContract(std::move(frame))
    → virtual dispatch
    → lock_guard on m_0 (teaching only; would remove in prod)
    → atomic index → slot
    → make_unique<TelemetryFrame> in memory_pool[slot]
    → source_device = FPGA_TRACKER | OSCILLOSCOPE
    → log
```
# unified_multiplex_stream
# unified_multiplex_stream
