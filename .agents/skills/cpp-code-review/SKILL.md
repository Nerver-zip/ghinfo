---
name: cpp-code-review
description: Perform final review for correctness, secret safety, API compatibility, simplicity, and test evidence.
---

# C++ Code Review

Review in this order:

1. state correctness;
2. secret handling;
3. API/schema compatibility;
4. failure semantics;
5. ownership/lifetimes/concurrency;
6. unnecessary dependencies/abstractions;
7. tests and validation evidence.

Look specifically for:

- GitHub calls from request handlers;
- snapshots mutated after publication;
- PAT/header logging;
- partial-state publication;
- unbounded retries;
- schema strings inferred from enum ordinals;
- unnecessary databases, queues, or async frameworks.
