#!/usr/bin/env bash
set -euo pipefail

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
python3 -m unittest discover -s scripts -p 'test_kustom_setup.py' -q
./scripts/check-format.sh

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git diff --check
fi
