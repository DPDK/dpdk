#!/bin/bash
#
# Build script for DPDK vHost fuzzer
#
# This script builds the fuzzer in multiple configurations:
# 1. AFL++ instrumented with AddressSanitizer
# 2. Standalone mode for manual testing
# 3. LibFuzzer mode (optional)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "================================================"
echo "DPDK vHost Descriptor Fuzzer - Build Script"
echo "================================================"
echo ""

# Check for AFL++
if ! command -v afl-clang-fast &> /dev/null; then
    echo "[WARNING] AFL++ not found. Building standalone version only."
    AFL_AVAILABLE=0
else
    echo "[+] AFL++ found: $(which afl-clang-fast)"
    AFL_AVAILABLE=1
fi

# Build standalone version
echo ""
echo "[+] Building standalone version (for manual testing)..."
gcc -o vhost_fuzzer_standalone \
    -g -O1 \
    -Wall -Wextra \
    -fsanitize=address \
    -fsanitize=undefined \
    -fno-omit-frame-pointer \
    vhost_descriptor_fuzzer.c

if [ $? -eq 0 ]; then
    echo "[✓] Standalone fuzzer built: vhost_fuzzer_standalone"
else
    echo "[✗] Failed to build standalone fuzzer"
    exit 1
fi

# Build AFL++ version if available
if [ $AFL_AVAILABLE -eq 1 ]; then
    echo ""
    echo "[+] Building AFL++ instrumented version..."

    export AFL_USE_ASAN=1
    export AFL_USE_UBSAN=1

    afl-clang-fast -o vhost_fuzzer_afl \
        -g -O1 \
        -fsanitize=address \
        -fsanitize=undefined \
        -fno-omit-frame-pointer \
        vhost_descriptor_fuzzer.c

    if [ $? -eq 0 ]; then
        echo "[✓] AFL++ fuzzer built: vhost_fuzzer_afl"
    else
        echo "[✗] Failed to build AFL++ fuzzer"
        exit 1
    fi
fi

echo ""
echo "================================================"
echo "Build complete!"
echo "================================================"
echo ""
echo "Available binaries:"
echo "  - vhost_fuzzer_standalone: Manual testing"
if [ $AFL_AVAILABLE -eq 1 ]; then
    echo "  - vhost_fuzzer_afl:        AFL++ fuzzing"
fi
echo ""
echo "Next steps:"
echo "  1. Run standalone tests: ./vhost_fuzzer_standalone"
echo "  2. Generate corpus: python3 generate_corpus.py"
if [ $AFL_AVAILABLE -eq 1 ]; then
    echo "  3. Start fuzzing: ./run_fuzzer.sh"
fi
echo ""
