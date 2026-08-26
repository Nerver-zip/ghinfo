---
name: cpp-testing
description: Add deterministic unit, integration, golden-schema, and failure-mode tests for ghinfo.
---

# C++ Testing

## Strategy

Prioritize observable contracts:

1. config validation;
2. GitHub payload normalization;
3. pagination and conditional requests;
4. rate-limit/error mapping;
5. last-known-good snapshot behavior;
6. public API JSON golden tests;
7. readiness/liveness semantics.

Prefer fixtures to live network tests.

Never make the default test suite depend on GitHub availability or a real PAT.

## Completion

- positive case;
- negative/malformed case;
- failure behavior;
- deterministic output where applicable.
