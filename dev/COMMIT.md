# Commit Handoff

## Objective

Harden GitHub parsing and refresh failure behavior for MVP-014.

## Files changed

- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `tests/snapshot_builder_test.cpp`
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- Timeout, rate-limit hints, 64-bit IDs, nullable job timestamps, and partial
  refresh behavior have regression coverage.
- Existing 304, pagination, malformed JSON, secret-safety, and API golden
  coverage remains green.
- Nullable GitHub fields are represented as absent optionals and serialize as
  JSON `null` where applicable.
- Documentation records the all-or-nothing candidate policy.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 38 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  pending before commit; this executor runs under ptrace and otherwise makes
  LeakSanitizer abort during test discovery.
- `git diff --check` — pending before commit.

## Compatibility and security

No public API shape changed. The parser remains strict about required fields,
uses fixed-width IDs, preserves nullability, and keeps upstream response bodies
out of exception text.

## Deferred

Production container verification, signal behavior, CI/release work, and the
final v0.1.0 audit remain.
