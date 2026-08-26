# Commit Handoff

## Objective

Reject malformed upstream timestamps and repository identity mismatches at the
GitHub boundary, with regression coverage.

## Files changed

- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `docs/TESTING.md`
- `dev/PLAN.md`
- `dev/COMMIT.md`

## Acceptance criteria

- GitHub timestamps are constrained to the public UTC ISO-8601 form.
- Repository responses must identify the requested repository.
- Invalid timestamp and identity payloads are reported as semantic errors.
- Existing live HTTP endpoint coverage remains green.

## Validation

- `./scripts/validate.sh` — passed.
- `cmake --build --preset dev --parallel` and `ctest --preset dev
  --output-on-failure` — 40 tests passed.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan --parallel` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  40 tests passed. LeakSanitizer remained disabled because this host does not
  permit the sanitizer runtime's ptrace operation.
- `cmake --preset release && cmake --build --preset release --parallel` —
  passed.
- `actionlint .github/workflows/*.yml` — passed.
- `docker compose config --quiet` — passed with expected warnings for unset
  runtime variables.
- `git diff --check` — passed.

## Compatibility and security

No public schema changed. The parser only rejects malformed untrusted upstream
data; tests use loopback and synthetic payloads without credentials.

## Deferred

Docker image build and remote GitHub Actions/release verification still require
external Docker/root and GitHub remote/authentication state. The current
checkout has no configured Git remote and `gh` is unauthenticated.
