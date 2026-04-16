# Design Decisions

Decisions not specified in the original spec, recorded for reviewers and future maintainers.

---

## Arg parsing

**Unknown `--mv-*` flags are an error, not passed to Valgrind.**
Rationale: Valgrind does not understand any `--mv-*` flag and would exit with an error
anyway.  Failing early with a clear message is friendlier.

**Valgrind flags are assumed to use `=` for values (no space-separated values).**
Rationale: Valgrind's entire CLI uses `--flag=value` syntax.  A space-separated value
(e.g. `--log-file mylog`) would be misinterpreted as `--log-file` being a flag and
`mylog` being the target.  This matches real Valgrind behaviour; users who try it will
get a confusing-but-fixable error from Valgrind itself.

---

## File classification

**`.C` (uppercase) is treated as C++ source (GCC convention).**
Rationale: `gcc` routes `.C` files to the C++ compiler.  Following GCC avoids
surprising students who name their files that way.  Noted in `--help` output.

**Extension beats magic bytes.**
Rationale: If a user names a file `foo.c`, they expect it to be compiled as C regardless
of what bytes happen to be in it.  This matches how every build system works.

---

## Docker image name

**Image is named `mvalgrind-ubuntu:latest`.**
Rationale: Descriptive enough that students can find it in `docker images` without
confusion.  Unlikely to collide with images they already have.

---

## `ensure_image` uses `std::system()` for inspect and build

**`std::system()` is used for `docker image inspect` (existence check) and `docker build` (first run).**
Rationale: The spec explicitly permits this.  For the existence check we redirect to
`/dev/null`; for the build we let output flow to the terminal so users see progress.
`docker run` itself uses fork/exec as required.

**Docker availability is checked via `docker info > /dev/null 2>&1`.**
Rationale: `docker info` confirms both that the binary is on PATH and that the daemon is
running.  A "binary exists but daemon is down" situation is common on Macs when Docker
Desktop hasn't been launched; `docker image inspect` would succeed but `docker run` would
fail later with a cryptic error.

---

## TTY detection for `docker run`

**`-i` is passed when `STDIN_FILENO` is a TTY; `-t` when `STDOUT_FILENO` is a TTY.**
Rationale: Mirrors how most interactive tools wrap Docker.  Without `-i`, programs that
read stdin would block forever.  Without `-t`, Valgrind and GCC suppress colour output
and progress indicators.

---

## Compilation flags

**Source files are always compiled with `-g -O0 -Wall -Wextra`.**
Rationale: `-g` is required for Valgrind to report file names and line numbers.  `-O0`
prevents optimisations that eliminate the very bugs students are trying to find.  The
`-Wall -Wextra` surface extra issues while the file is being compiled.  No mechanism is
provided for students to add extra `-I` paths or `-D` defines; multi-directory projects
must pass an ELF binary instead.

---

## No Homebrew bottles in v0.1.0

**The formula downloads a pre-built binary tarball via `on_arm` / `on_intel` blocks rather than using Homebrew's bottle infrastructure.**
Rationale: Bottle infrastructure requires a bottle server (typically GitHub Packages via
`brew bottle`).  Pre-built tarballs attached to GitHub Releases are simpler for a
personal tap and still give users a fast, no-compile install.  Bottles can be added later
once the tap is established.

---

## GitHub username placeholder

**The formula and release workflow use `shaansriram` as the GitHub username.**
Rationale: Derived from the author's email address.  Update if the repo lives under a
different account or org.

---

## Exit codes

**mvalgrind orchestration errors exit with code 2.**
Rationale: Valgrind uses 0 for success and 1 for "errors detected"; reserving 2 for
mvalgrind's own errors avoids ambiguity.  `128 + signal_number` is used for signal
termination to match shell conventions.

---

## Signal handling: `signal()` inside signal handler

**`signal(sig, SIG_DFL)` is called inside the signal handler before `raise(sig)`.**
Rationale: The spec requires restoring the default handler on the second signal.
`sigaction` is strictly safer, but for the `SIG_DFL` assignment the operation is
effectively atomic on all platforms this tool targets.  The handler is simple enough
that the risk of the not-formally-async-signal-safe `signal()` call is negligible.

---

## Catch2 version

**Catch2 v3.5.2 is pinned in `tests/CMakeLists.txt`.**
Rationale: v3 has a cleaner CMake integration than v2 (provides `Catch2::Catch2WithMain`)
and is header-friendly via FetchContent.  The version is pinned to avoid unexpected
breakage from upstream changes.
