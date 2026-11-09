# Scaling Merge Sort

Scaling Merge Sort is an empirical High-Performance Computing (HPC) research suite that benchmarks and microarchitecturally diagnoses parallel merge sort implementations in C++20. The project analyzes performance scaling bottlenecks across multi-core processors, focusing on the serial merge limitation formalized by Amdahl's Law and off-chip memory bandwidth saturation ("The Memory Wall") modeled via the UC Berkeley Roofline Model.

## Architecture

- **Core Engine:** C++20 with native machine vectorization and link-time optimization (`-O3 -march=native -flto`)
- **Parallel Runtime:** OpenMP 4.5 task parallelism (`#pragma omp task`) with dynamic work-stealing
- **Microarchitectural Profiling:** Linux hardware Performance Monitoring Units (PMUs) via `perf stat`
- **Analytical Model:** UC Berkeley Empirical Roofline Model ($I = \text{Ops/Byte}$ vs. attainable memory bandwidth)
- **Build System:** CMake 3.22+

## Requirements

For local development and profiling:

- Ubuntu Linux (Kernel 5.15+ recommended)
- GCC 13+ or Clang 16+ supporting C++20
- CMake 3.22+
- OpenMP 4.5 runtime libraries (`libomp-dev`)
- Linux `perf` diagnostic tools (`linux-tools-common`, `linux-tools-generic`, `linux-tools-$(uname -r)`)

## Run Automated Benchmarks

From the project root:

```bash
bash scripts/run_benchmarks.sh
```

The benchmark matrix evaluates:

| Parameter | Values |
| --- | --- |
| Variants | 1 (Serial), 2 (Task Divide / Serial Merge), 3 (Blelloch Merge) |
| Distributions | `uniform`, `sorted`, `reverse`, `organ_pipe` |
| Thread Counts | 1, 2, 4, 6 |
| Input Size | 10,000,000 elements (64-bit integers) |

To view generated telemetry data:

```bash
ls -lh benchmarks/results/
cat benchmarks/results/benchmark_run_*.csv
```

The automation script captures hardware PMU counters into timestamped CSV files. Ensure unprivileged counter access is enabled beforehand using `sudo sysctl -w kernel.perf_event_paranoid=-1`.

## Run Locally

1. Enable unprivileged access to bare-metal hardware Performance Monitoring Units (PMUs):

```bash
sudo sysctl -w kernel.perf_event_paranoid=-1
```

2. Configure and build the benchmark executable:

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

3. Run the baseline serial sort (Variant 1):

```bash
./sort_benchmark --variant=1 --size=10000000 --threads=1 --dist=uniform
```

Run the task-parallel divide sort with serial merge (Variant 2):

```bash
./sort_benchmark --variant=2 --size=10000000 --threads=6 --dist=uniform
```

Run the lock-free dual binary-search parallel merge sort (Variant 3):

```bash
./sort_benchmark --variant=3 --size=10000000 --threads=6 --dist=uniform
```

To run hardware counter diagnostics on any configuration using Linux `perf`:

```bash
perf stat -e cycles,instructions,cache-misses,LLC-load-misses,LLC-store-misses \
  ./sort_benchmark --variant=3 --size=10000000 --threads=6 --dist=uniform
```

## Command-Line Arguments

All benchmark runs use `./build/sort_benchmark` relative to the project root.

Parameters are passed via command-line flags:

```bash
./build/sort_benchmark --variant=3 --size=10000000 --threads=6 --dist=uniform
```

Supported arguments:

- `--variant={1|2|3}` - sorting implementation tier (default: `1`)
- `--size={N}` - number of 64-bit signed integers (`int64_t`) to sort (default: `50000000`)
- `--threads={P}` - number of OpenMP worker threads (default: `1`)
- `--dist={TYPE}` - synthetic input distribution (default: `uniform`)
- `--silent` - suppress human-readable stdout for automated scripting

Example output:

```text
========================================================
[✓] Variant:      3
[✓] Elements:     10000000
[✓] Threads:      6
[✓] Distribution: uniform
[✓] Sorted:       PASSED
[✓] Checksum:     MATCHED
[✓] Wall Time:    0.184321 seconds
[✓] Throughput:   54.2532 M elements/sec
========================================================
```

## Algorithmic Variants

### Variant 1: Optimized Serial Baseline

- Recursive divide-and-conquer using an upfront pre-allocated auxiliary scratchpad
- Switches to `std::sort` for sub-problems where $n \le 64$ to remove small recursive call overhead
- Serves as the single-threaded baseline reference ($T_1$)

### Variant 2: Task-Parallel Divide with Serial Merge

- Decomposes recursive partitions into dynamic OpenMP tasks (`#pragma omp task`)
- Leaves the final recombining merge stage strictly single-threaded
- Demonstrates Amdahl's Law and measures the upper bound on multi-core speedup

### Variant 3: Lock-Free Blelloch Parallel Merge

- Dual binary-search rank partitioning (`std::lower_bound`) bisects both sorted subarrays
- Concurrently writes merged elements directly into disjoint output slices
- Eliminates mutex locks, atomic operations, and critical section bottlenecks to expose memory bandwidth limits

### Input Distributions

Synthetic datasets generated via `std::mt19937_64`:

- `uniform` - random 64-bit signed integers maximizing branch entropy
- `sorted` - ascending sequence testing streaming memory access and stride prefetchers
- `reverse` - descending sequence forcing continuous interleaved merges
- `organ_pipe` - ascending and then descending data evaluating partition balance
- `duplicates` - low-entropy keys ($[0, 15]$) validating boundary edge cases

Typical flow:

1. The test harness creates an arena for both primary and auxiliary memory buffers.
2. A first-touch initialization pass pre-faults virtual memory pages into physical RAM.
3. The selected sorting variant executes under a high-resolution monotonic timer.
4. The output is validated for monotonic non-decreasing order ($\forall i: A[i] \le A[i+1]$).
5. Multiset parity is verified against a reference sort using sum and XOR checksums.

## Telemetry Metrics

The automation script records microarchitectural PMU events to CSV:

```csv
timestamp,variant,distribution,threads,trial,array_size,time_sec,cycles,instructions,ipc,cache_misses,llc_load_misses,llc_store_misses,branch_misses
```

Operational intensity ($I$) is computed for the Roofline Model using Last-Level Cache misses:

$$I = \frac{\text{Comparisons / Operations}}{(\text{LLC-load-misses} + \text{LLC-store-misses}) \times 64\text{ bytes}} \quad [\text{Ops/Byte}]$$

## Configuration and Performance Notes

Pin OpenMP threads to physical processor cores to avoid cache thrashing caused by thread migration across Core Complexes (CCX). Before profiling, configure your environment with:

```bash
export OMP_PROC_BIND=spread
export OMP_PLACES=threads
```

Memory allocations must be pre-allocated before timing blocks to avoid recording Linux kernel page-fault overhead in benchmark metrics.

## Project Structure

```text
scaling-merge-sort/
├── include/
│   ├── data_generators.hpp     Synthetic distribution engines
│   ├── sort_variants.hpp       Interface declarations for Variants 1, 2, and 3
│   └── verifier.hpp            Sorted invariant and multiset checksum validator
├── src/
│   ├── main.cpp                CLI parser, harness, and timing driver
│   ├── serial_sort.cpp         Variant 1 implementation
│   ├── omp_divide_sort.cpp     Variant 2 implementation
│   └── omp_blelloch_sort.cpp   Variant 3 implementation
├── scripts/
│   └── run_benchmarks.sh       Automated thread sweep and perf stat logging script
├── benchmarks/
│   └── results/                Output directory for telemetry CSV files
├── CMakeLists.txt              CMake build configuration
└── README.md                   Project documentation
```

## License

No license has been specified for this project yet.
