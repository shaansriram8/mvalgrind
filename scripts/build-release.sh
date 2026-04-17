#!/usr/bin/env bash
# Build a stripped release binary and package it as a tarball.
# Run from the repository root.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel "$(sysctl -n hw.logicalcpu 2>/dev/null || nproc)"

strip build-release/macgrind

VERSION="$(cmake -B build-release -S . --no-warn-unused-cli -L 2>/dev/null \
    | grep CMAKE_PROJECT_VERSION | head -1 | cut -d= -f2 | tr -d ' ')"
ARCH="$(uname -m)"
TARBALL="macgrind-${VERSION}-${ARCH}-apple-darwin.tar.gz"

tar -czf "$TARBALL" -C build-release macgrind
shasum -a 256 "$TARBALL"

echo ""
echo "Created: $TARBALL"
