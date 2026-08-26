#!/usr/bin/env bash
set -euo pipefail

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
./scripts/check-format.sh

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git diff --check
fi
