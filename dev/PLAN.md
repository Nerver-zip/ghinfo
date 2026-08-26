# Active Plan

## Current milestone

**MVP-014 — Hardening**

Target commit:

```text
test: harden GitHub failure and API contracts
```

## Goal

Close the high-risk failure and contract gaps before container and release
work, with deterministic local regression evidence.

## Acceptance criteria

- Transport tests cover 304, 403/429, timeout, malformed JSON, pagination,
  rate-limit hints, and secret-safe errors.
- Domain parsing preserves 64-bit IDs and nullable upstream fields.
- Snapshot construction rejects partial repository refreshes without exposing
  partial candidates.
- API golden and resource tests cover schema and normalized output.
- Documentation states the all-or-nothing refresh policy and current test
  coverage.

## Non-goals

- new product endpoints;
- persistence;
- changes to public v1 meanings;
- container or CI redesign.

## Expected files

- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `tests/snapshot_builder_test.cpp`
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Required skills

- `.agents/skills/modern-cpp/SKILL.md`
- `.agents/skills/github-rest/SKILL.md`
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/api-contract/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan
LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure
git diff --check
```
