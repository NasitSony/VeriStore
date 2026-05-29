# Contributing to VeriStore

Thanks for your interest in contributing! VeriStore is a correctness-first storage engine — contributions that improve durability, recovery, or consistency guarantees are especially welcome.

## Getting Started

### Prerequisites
- C++17 compiler (GCC or Clang)
- CMake 3.15+

### Local Setup
```bash
git clone https://github.com/NasitSony/VeriStore.git
cd VeriStore
cmake -S . -B build
cmake --build build
./build/kv_cli
```

### Run the Demos
```bash
./build/raft_demo              # Raft replication demo
./build/object_store_write_demo   # Object storage write
./build/object_store_recover_demo # Object storage recovery
```

## How to Contribute

### Reporting Bugs
Open an issue at [GitHub Issues](https://github.com/NasitSony/VeriStore/issues) and include:
- What you expected to happen
- What actually happened
- Steps to reproduce
- Relevant logs or error messages

### Suggesting Features
Open an issue with the `enhancement` label. Good feature requests explain:
- The problem you're trying to solve
- How correctness guarantees are preserved

### Submitting a Pull Request
1. Fork the repo
2. Create a branch: `git checkout -b your-feature-name`
3. Make your changes
4. Make sure the build passes: `cmake -S . -B build && cmake --build build`
5. Commit with a clear message: `git commit -m "feat: add X"`
6. Push and open a PR against `main`

## Good First Issues
Look for issues tagged `good first issue`. Areas that always welcome contributions:
- Additional failure scenario tests
- Performance benchmarks
- Documentation improvements
- CLI enhancements
- Garbage collection improvements

## Code Style
- Follow C++17 conventions
- Keep correctness guarantees intact
- Add comments for non-obvious logic
- Match existing patterns in the codebase

## Commit Message Format
- `feat:` — new feature
- `fix:` — bug fix
- `docs:` — documentation only
- `perf:` — performance improvement
- `test:` — adding or updating tests
- `refactor:` — code cleanup

## Questions?
Open an issue — happy to help you get oriented.
