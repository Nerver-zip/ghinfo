---
name: modern-cpp
description: Implement safe, small C++23 code using RAII, value semantics, strong domain types, and explicit ownership.
---

# Modern C++

## Procedure

1. Establish invariants and ownership before choosing classes.
2. Prefer values and composition.
3. Make invalid domain states difficult to represent.
4. Use standard library facilities before custom abstractions.
5. Keep allocations and polymorphism out of hot/simple paths unless justified.
6. Keep exception/lifetime behavior explicit.
7. Compile with strict warnings.
8. Verify sensitive work with ASan/UBSan.

## Checklist

- ownership is unambiguous;
- no dangling views;
- no hidden global mutable state;
- conversions are explicit;
- enums serialize explicitly;
- no premature optimization.
