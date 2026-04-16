#!/usr/bin/env bash
# Bump the Homebrew formula to a new version.
# Usage: update-homebrew.sh <version> <arm64-sha256> <x86_64-sha256>
#
# Example (called from release.yml after artifacts are built):
#   ./scripts/update-homebrew.sh 0.2.0 abc123... def456...
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <version> <arm64-sha256> <x86_64-sha256>" >&2
    exit 1
fi

VERSION="$1"
ARM64_SHA="$2"
X86_SHA="$3"

FORMULA="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/homebrew/mvalgrind.rb"

if [[ ! -f "$FORMULA" ]]; then
    echo "ERROR: formula not found at $FORMULA" >&2
    exit 1
fi

# Bump version line
sed -i.bak "s/version \"[^\"]*\"/version \"${VERSION}\"/" "$FORMULA"

# Bump URLs (arm64 and x86_64)
sed -i.bak \
    "s|releases/download/v[^/]*/mvalgrind-[^-]*-arm64|releases/download/v${VERSION}/mvalgrind-${VERSION}-arm64|g" \
    "$FORMULA"
sed -i.bak \
    "s|releases/download/v[^/]*/mvalgrind-[^-]*-x86_64|releases/download/v${VERSION}/mvalgrind-${VERSION}-x86_64|g" \
    "$FORMULA"

# Bump SHA256 placeholders / previous values.
# The file has exactly two sha256 lines inside on_arm / on_intel blocks.
python3 - "$FORMULA" "$ARM64_SHA" "$X86_SHA" <<'PY'
import sys, re

path, arm64, x86 = sys.argv[1], sys.argv[2], sys.argv[3]
content = open(path).read()

# Replace the sha256 inside the on_arm block
content = re.sub(
    r'(on_arm\b.*?sha256 ")[^"]*(")',
    lambda m: m.group(1) + arm64 + m.group(2),
    content, count=1, flags=re.DOTALL,
)
# Replace the sha256 inside the on_intel block
content = re.sub(
    r'(on_intel\b.*?sha256 ")[^"]*(")',
    lambda m: m.group(1) + x86 + m.group(2),
    content, count=1, flags=re.DOTALL,
)
open(path, "w").write(content)
print(f"Updated {path}: arm64={arm64[:12]}... x86_64={x86[:12]}...")
PY

rm -f "${FORMULA}.bak"
echo "Formula updated to v${VERSION}."
