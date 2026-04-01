# grip

C++ all-in-one build tool & package manager.

Built with C++20. Direct compiler invocation (g++/clang++), TOML configuration, self-hosted package registry, Linux-first.

[한국어](,,/README.md)

## Quick Start

```bash
# Create a new project
grip new myproject
cd myproject

# Build
grip build

# Run
grip run

# Clean
grip clean
```

## grip.toml

```toml
[project]
name = "myapp"
version = "0.1.0"
standard = "c++20"
compiler = "g++"

[build]
sources = ["src"]
include = ["include"]
output = "build"
type = "bin"

[dependencies]
fmt = "10.1.0"
spdlog = "1.12.0"

[profile.debug]
opt_level = "0"
debug = true
flags = ["-Wall", "-Wextra"]

[profile.release]
opt_level = "2"
debug = false
flags = ["-Wall", "-DNDEBUG"]

[target.aarch64-linux-gnu]
compiler = "aarch64-linux-gnu-g++"
ar = "aarch64-linux-gnu-ar"
```

## Features

### Incremental Build

grip tracks `#include` dependencies and compares mtime to skip unchanged files. Only modified sources are recompiled, and object files are linked separately.

### Parallel Build

Compilation units are distributed across a thread pool using `std::thread::hardware_concurrency()`. Independent source files compile simultaneously.

### Debug / Release Profiles

```bash
grip build              # debug: -O0 -g
grip build --release    # release: -O2 -DNDEBUG
```

Output is separated by profile: `build/debug/`, `build/release/`.

Define custom profiles in `grip.toml` under `[profile.debug]` and `[profile.release]` with `opt_level`, `debug`, and `flags`.

### Package Manager

Cargo-style dependency management with a self-hosted registry server.

```bash
# Install a specific version
grip install fmt@10.1.0

# Install latest version
grip install fmt
```

Packages are stored locally in `grip_modules/{name}/{version}/`. Dependencies declared in `grip.toml` are automatically installed on `grip build`.

Features:
- Recursive dependency resolution
- Header-only library support (empty `source_dir`)
- Per-package compiler flags
- Static library (`.a`) generation
- `compile_flags.txt` auto-generation for IDE support

### grip.lock

Running `grip build` generates a `grip.lock` file that snapshots the exact dependency tree. When `grip.lock` is present, builds use locked versions instead of resolving from the registry.

```toml
[[package]]
name = "fmt"
version = "10.1.0"

[[package]]
name = "spdlog"
version = "1.12.0"
dependencies = ["fmt@10.1.0"]
```

### grip test

```bash
grip test
```

Discovers `.cpp` files in `tests/`, builds each as an independent binary (linked against project objects and dependencies), and runs them. Exit code 0 = PASS, non-zero = FAIL.

```
[PASS] test_math
[FAIL] test_parser
Results: 1/2 passed
```

### Cross-Compilation

```bash
grip build --target aarch64-linux-gnu
```

Define target toolchains in `grip.toml` under `[target.<triple>]`. Packages are built per-target with separate `obj/` and `lib/` directories, so native and cross builds coexist without conflicts.

## Project Structure

```
myproject/
├── grip.toml
├── grip.lock
├── compile_flags.txt
├── include/
│   └── myproject/
├── src/
│   └── main.cpp
├── tests/
│   └── test_basic.cpp
├── grip_modules/
│   └── fmt/
│       └── 10.1.0/
└── build/
    ├── debug/
    └── release/
```

## Registry

grip uses a self-hosted registry server (built with yNet) that serves package metadata as JSON.

**Endpoints:**
- `GET /packages/:name` — package info with available versions
- `GET /packages/:name/:version` — version-specific metadata with download URL

**package.json format:**
```json
{
    "name": "spdlog",
    "version": "1.12.0",
    "url": "https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz",
    "include_dir": "include",
    "source_dir": "src",
    "exclude": [],
    "flags": ["-DSPDLOG_COMPILED_LIB", "-DSPDLOG_FMT_EXTERNAL"],
    "dependencies": ["fmt@10.1.0"]
}
```

## License

Apache-2.0
