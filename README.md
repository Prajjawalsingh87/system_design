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

GitHub Actions ([build.yml](.github/workflows/build.yml)) compiles every `.cpp` file on each push and pull request. A file without `main` is treated as intentionally partial and compiled with `-c`.

## Roadmap

These topics are planned and not implemented yet. Each one will get its own folder and a row in the table above.

| Area | Planned topics |
|---|---|
| OOP concepts | Inheritance, polymorphism |
| Low-level design (`lld/`) | Parking lot, library management, elevator, hotel booking, Splitwise clone, ATM, snake and ladder, tic tac toe, food delivery |
| System components (`hld/`, `cache/`) | Rate limiter, load balancer, URL shortener, notification service, chat system, logging system, task scheduler, LRU/LFU cache, pub-sub |
| Concurrency (`concurrency/`) | Threads, mutexes and locks, thread pool, producer-consumer, deadlock handling |
| Other | Design patterns, smart pointers and memory management, TCP/IP and socket basics, database indexing concepts |

Planned layout:

```text
system_design/
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

Issues and pull requests are welcome. For a new example, add a self-contained `.cpp` file under the matching folder (use `snake_case` file names) and add a row to the table in "What's implemented".

## Author

Prajjawal Singh: <https://github.com/Prajjawalsingh87>

## License

Intended to be open source under the MIT License. A `LICENSE` file has not been added yet.


## One-command local verification

Run `python3 scripts/build.py` from any directory. Requires Python 3 and GCC with
C++17 support. Set `CXX` to choose another GCC executable. Binaries keep their
source directory structure under `build/`, so exercises with the same filename
do not overwrite one another. Partial examples without `main` compile to objects.
The command compiles examples; it does not run programs that may wait for stdin.

These are native C++ exercises, not web applications; Vercel hosting is not applicable.
