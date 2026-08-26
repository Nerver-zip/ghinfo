---
name: cmake-build
description: Maintain the ghinfo CMake build, dependency pins, presets, warnings, and sanitizer configuration.
---

# CMake Build

## Rules

- Use target-based CMake.
- Keep external dependency versions pinned.
- Prefer system libcurl.
- Keep project warnings on project targets, not third-party targets.
- Do not add a dependency without documenting purpose and runtime/build impact.
- Keep `dev`, `release`, and `asan` presets working.

## Verification

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```
