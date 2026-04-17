# mvalgrind

Run Valgrind on Apple Silicon Macs — no Linux machine required.

---

## Why

Valgrind doesn't run natively on macOS. Students debugging C/C++ programs have two
painful workarounds: `-fsanitize=address` (different error messages, different semantics)
or SSH into a Linux server (needs a network, needs an account, adds friction at exactly
the wrong moment).

`mvalgrind` is a single binary that transparently runs the real Valgrind inside a local
Docker container. The directory containing your source file is bind-mounted read-only at
`/work` so sibling headers and data files are automatically available. From your
terminal the output is identical to what you'd see on a Linux machine.

---

## Install

**Homebrew (recommended)**

```bash
brew tap shaansriram8/mvalgrind
brew install mvalgrind
```

> Docker Desktop must be installed and running. The `docker` package that Homebrew
> installs provides only the CLI client; the daemon comes with
> [Docker Desktop](https://www.docker.com/products/docker-desktop/).

On the first invocation `mvalgrind` builds a small Ubuntu 22.04 image with Valgrind,
GCC, and G++ (~200 MB download). Every subsequent run reuses the cached image and
starts in a few seconds.

### Build from source

```bash
git clone https://github.com/shaansriram8/mvalgrind.git
cd mvalgrind
cmake -B build -S .
cmake --build build
# Optionally copy to a directory on your PATH:
sudo cp build/mvalgrind /usr/local/bin/
```

Requirements: CMake ≥ 3.16, a C++17 compiler, Docker Desktop.

---

## Usage

```bash
# Check for memory leaks in a C source file (auto-compiled inside the container)
mvalgrind --leak-check=full ./leak.c

# Detect uninitialised reads in a C++ file
mvalgrind --track-origins=yes ./uninit.cpp

# Run against a prebuilt Linux ELF binary
mvalgrind --leak-check=full ./my_linux_binary arg1 arg2

# Pass arguments to your program (anything after the target goes to the program)
mvalgrind --leak-check=full ./my_prog -n 100 input.txt

# Show the docker command that will be run (debugging aid)
mvalgrind --mv-verbose --leak-check=full ./leak.c
```

All Valgrind flags (`--tool=`, `--show-leak-kinds=`, `--suppressions=`, etc.) are
forwarded unchanged.

`mvalgrind`-specific flags (will never collide with Valgrind):

| Flag                  | Effect                                                  |
| --------------------- | ------------------------------------------------------- |
| `--mv-keep`           | Don't `--rm` the container on exit                      |
| `--mv-verbose`        | Print the `docker run` command before running           |
| `--mv-rebuild-image`  | Delete and rebuild the cached Docker image from scratch |
| `-h`/`--help`         | Show help                                               |
| `-V`/`--version`      | Show version                                            |

---

## How it works

```
You on Mac         mvalgrind (C++ binary)         Docker container
──────────         ──────────────────────         ────────────────
mvalgrind          classifies target:             Ubuntu 22.04
--leak-check=full  .c/.cpp → compile then run     + valgrind
./leak.c      ──►  ELF     → run directly    ──►  + gcc / g++
                                                   + /work (your dir, :ro)
                   fork/exec docker run            │
Your terminal ◄──  stdio inherited (no pipes) ◄───┘
```

1. **Classify** — extension (`.c`, `.cpp`, …) or ELF magic bytes.
2. **Image** — on first run, build `mvalgrind-ubuntu:latest` from an embedded
   Dockerfile; subsequent runs skip this step.
3. **Mount** — the directory containing the target is bind-mounted read-only at `/work`.
4. **Run** — `docker run` is fork/exec'd; your terminal inherits its stdio so Valgrind's
   output appears exactly as if it were running natively. Valgrind's exit code
   propagates back to your shell unchanged, so autograders and scripts that check `$?`
   work correctly.
5. **Signals** — SIGINT/SIGTERM/SIGHUP force-remove the container via
   `docker rm -f` even if it is still booting.

---

## Limitations

- **Apple Silicon only.** Pre-built binaries are provided for arm64 (M1/M2/M3/M4). Intel Mac users must [build from source](#build-from-source).
- **Mac binaries don't work.** Mach-O executables (anything compiled on your Mac) cannot
  run inside a Linux container. Point `mvalgrind` at the source file and it will compile
  a Linux binary for you automatically.
- **First run is slow.** Building the Docker image takes 1–2 minutes depending on your
  internet connection. Every subsequent run is fast.
- **Requires Docker Desktop to be running.** If the Docker daemon isn't reachable,
  `mvalgrind` exits immediately with a helpful error.
- **Source files are recompiled on every run.** There is no caching of compiled binaries
  between invocations. This keeps correctness simple — you always run against your latest
  source — but means there is a small compile overhead each time.
- **Single-directory projects only (source mode).** The parent directory of the source
  file is mounted, so `#include "sibling.h"` works. Multi-directory projects should be
  compiled to a Linux ELF binary first (e.g. inside a CI container) and passed directly.

---

## Contributing

```bash
# Build and run unit tests (no Docker required)
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure

# Run integration tests (requires Docker)
DOCKER_AVAILABLE=1 MVALGRIND=./build/mvalgrind \
  bash tests/integration/run_integration.sh

# Check formatting
clang-format --dry-run --Werror src/*.cpp src/*.hpp
```

Coding style: Google C++ style, 4-space indent, 100-column lines. See `.clang-format`.

### Branch protection

`main` is a protected branch. All contributions must come in via a pull request — direct
pushes are blocked. Before a PR can be merged:

- **CI must pass** — both `Build & Test (ubuntu-latest)` and `Build & Test (macos-latest)`
  status checks are required.
- **Linear history** — merge commits are disabled; rebase or squash before merging.
- **No force-pushes** — the branch history is immutable after merge.

In practice: open a PR, make sure `ctest` and the clang-format lint step are green on both
platforms, and the merge button will unlock.

---

## License

MIT — see [LICENSE](LICENSE).
