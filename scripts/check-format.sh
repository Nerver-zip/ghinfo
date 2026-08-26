#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format is required" >&2
    exit 1
fi

mapfile -t files < <(find include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)

if ((${#files[@]} == 0)); then
    exit 0
fi

clang-format --dry-run --Werror "${files[@]}"
