#!/usr/bin/env bash
# Integration tests for macgrind.  Requires Docker and a built binary.
# Set DOCKER_AVAILABLE=1 to opt in; tests are skipped otherwise.
set -euo pipefail

if [[ "${DOCKER_AVAILABLE:-0}" != "1" ]]; then
    echo "SKIP: DOCKER_AVAILABLE is not set to 1 — integration tests skipped."
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MACGRIND="${MVALGRIND:-$REPO_ROOT/build/macgrind}"
FIXTURES="$SCRIPT_DIR/fixtures"
EXPECTED="$SCRIPT_DIR/expected"

if [[ ! -x "$MACGRIND" ]]; then
    echo "ERROR: macgrind binary not found at $MACGRIND"
    echo "  Build with: cmake -B build -S . && cmake --build build"
    exit 1
fi

pass=0
fail=0

run_test() {
    local name="$1"
    local pattern="$2"
    shift 2
    printf "  %-55s" "$name ..."
    local output
    output=$("$@" 2>&1 || true)
    if echo "$output" | grep -qF "$pattern"; then
        echo "PASS"
        pass=$((pass + 1))
    else
        echo "FAIL"
        echo "    Expected to find: $pattern"
        echo "    Actual output (last 10 lines):"
        echo "$output" | tail -10 | sed 's/^/      /'
        fail=$((fail + 1))
    fi
}

echo ""
echo "=== macgrind integration tests ==="
echo ""

# ── Source file compilation + Valgrind ───────────────────────────────────────

run_test "leak.c: detects 'definitely lost'" \
    "definitely lost" \
    "$MACGRIND" --leak-check=full "$FIXTURES/leak.c"

run_test "clean.c: reports 'no leaks are possible'" \
    "no leaks are possible" \
    "$MACGRIND" --leak-check=full "$FIXTURES/clean.c"

run_test "uninit.cpp: detects uninitialised read" \
    "Conditional jump or move depends on uninitialised value" \
    "$MACGRIND" --track-origins=yes "$FIXTURES/uninit.cpp"

# ── SIGINT cleanup ────────────────────────────────────────────────────────────

printf "  %-55s" "SIGINT: container removed within 3 seconds ..."

# Start a long-running job in the background.
"$MACGRIND" "$FIXTURES/sleep_loop.c" &
MV_PID=$!
CONTAINER="macgrind-run-${MV_PID}"

# Poll until the container actually appears — avoids a fixed sleep that would
# be either too short on a slow daemon or wastefully long on a fast one.
BOOT_DEADLINE=$(( $(date +%s) + 30 ))
while [[ $(date +%s) -lt $BOOT_DEADLINE ]]; do
    if docker ps -a --format '{{.Names}}' 2>/dev/null | grep -qx "$CONTAINER"; then
        break
    fi
    sleep 0.5
done

# Send SIGINT now that the container is confirmed running.
kill -INT "$MV_PID" 2>/dev/null || true

# Wait up to 5 seconds for the cleanup to complete.
DEADLINE=$(( $(date +%s) + 5 ))
FOUND=0
while [[ $(date +%s) -lt $DEADLINE ]]; do
    if docker ps -a --format '{{.Names}}' 2>/dev/null | grep -qx "$CONTAINER"; then
        FOUND=1
        sleep 0.5
    else
        FOUND=0
        break
    fi
done

if [[ $FOUND -eq 0 ]]; then
    echo "PASS"
    pass=$((pass + 1))
else
    echo "FAIL (container '$CONTAINER' still listed)"
    docker rm -f "$CONTAINER" 2>/dev/null || true
    fail=$((fail + 1))
fi

# ── Summary ───────────────────────────────────────────────────────────────────

echo ""
echo "Results: $pass passed, $fail failed"
echo ""

[[ $fail -eq 0 ]]
