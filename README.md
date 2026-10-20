# System Design in C++

[![Build](https://github.com/Prajjawalsingh87/system_design/actions/workflows/build.yml/badge.svg)](https://github.com/Prajjawalsingh87/system_design/actions/workflows/build.yml)

This is where I learn and implement system design in modern C++: object-oriented foundations first, then low-level design (LLD) problems, system components, and concurrency.

## About

Hi, I'm Prajjawal. I use this repository to learn and practise:

- Object-oriented design and the four OOP pillars
- Low-level design (LLD) problems
- High-level design (HLD) components
- Design patterns in C++
- Multithreading and concurrency
- Scalable backend architecture

The goal is to understand how large systems are designed internally and to implement them in clean, efficient C++.

## What's implemented

### OOP concepts

| Concept | File | Key idea |
|---|---|---|
| Abstraction | [oop_concepts/abstraction.cpp](oop_concepts/abstraction.cpp) | An abstract `Car` interface (pure virtual methods) says *what* a car can do. The concrete `SportsCar` implements *how*. Callers only use a `Car*`. |
| Encapsulation | [oop_concepts/encapsulation.cpp](oop_concepts/encapsulation.cpp) | `SportsCar` keeps its state `private` and exposes getters and validated methods, so speed and gear can never be set to invalid values from outside. |

### System components

Each component is a header-only implementation with its own README (what problem it solves in real systems, and the trade-offs) and a `*_test.cpp` test executable that CI builds and runs.

| Component | Folder | Key idea |
|---|---|---|
| URL shortener core | [hld/url_shortener/](hld/url_shortener/) | Base62 encode/decode, a Snowflake-style ID generator with an injectable clock, and an in-memory store with TTL expiry, click counting and collision-safe custom aliases. |
| Consistent hashing | [hld/consistent_hashing/](hld/consistent_hashing/) | A hash ring with virtual nodes, add/remove node, key lookup, replica placement, and rebalancing stats: adding a 5th node moves ~20% of keys, versus ~80% with `hash % N`. |
| LFU + write-through cache | [cache/](cache/) | An O(1) LFU cache in front of a fake slow store. Writes are write-through. Measured hit ratio: 66% on Zipf traffic with a cache holding 5% of the keys. |

## Build and run

You need `g++` with C++17 support.

```bash
git clone https://github.com/Prajjawalsingh87/system_design.git
cd system_design

mkdir -p build
g++ -std=c++17 -Wall oop_concepts/abstraction.cpp -o build/abstraction
./build/abstraction
```

To compile every example at once:

```bash
mkdir -p build
failed=0
while read -r f; do
  g++ -std=c++17 -Wall "$f" -o "build/$(basename "$f" .cpp)" || { echo "FAILED: $f"; failed=1; }
done < <(find . -name '*.cpp' -not -path './.git/*')
[ "$failed" -eq 0 ]
```

To build and run every test (any file named `*_test.cpp`):

```bash
mkdir -p build
failed=0
for f in $(find . -name '*_test.cpp' -not -path './.git/*'); do
  bin="build/$(basename "$f" .cpp)"
  g++ -std=c++17 -Wall "$f" -o "$bin" && "$bin" || { echo "FAILED: $f"; failed=1; }
done
[ "$failed" -eq 0 ]
```

Tests use [testing/mini_test.hpp](testing/mini_test.hpp), a tiny header-only framework in this repo, so nothing is downloaded at build time.

GitHub Actions ([build.yml](.github/workflows/build.yml)) compiles every `.cpp` file on each push and pull request, then runs every `*_test.cpp` binary. A failing test fails the build. A file without `main` is treated as intentionally partial and compiled with `-c`.

## Roadmap

These topics are planned and not implemented yet. Each one will get its own folder and a row in the tables above.

| Area | Planned topics |
|---|---|
| OOP concepts | Inheritance, polymorphism |
| Low-level design (`lld/`) | Parking lot, library management, elevator, hotel booking, Splitwise clone, ATM, snake and ladder, tic tac toe, food delivery |
| System components (`hld/`, `cache/`) | Rate limiter, load balancer, notification service, chat system, logging system, task scheduler, LRU cache, pub-sub (done: URL shortener, consistent hashing, LFU + write-through cache) |
| Concurrency (`concurrency/`) | Threads, mutexes and locks, thread pool, producer-consumer, deadlock handling |
| Other | Design patterns, smart pointers and memory management, TCP/IP and socket basics, database indexing concepts |

Planned layout:

```text
system_design/
├── testing/        # mini_test.hpp, the header-only test framework
├── oop_concepts/
├── lld/
├── hld/
├── cache/
├── concurrency/
├── design_patterns/
├── networking/
└── databases/
```

## Contributing

Issues and pull requests are welcome. For a new example, add a self-contained `.cpp` file under the matching folder (use `snake_case` file names) and add a row to the table in "What's implemented". For a component, put the implementation in a header, add a `<name>_test.cpp` that ends with `MINI_TEST_MAIN()`, and CI will run it.

## Author

Prajjawal Singh: <https://github.com/Prajjawalsingh87>

## License

Intended to be open source under the MIT License. A `LICENSE` file has not been added yet.
