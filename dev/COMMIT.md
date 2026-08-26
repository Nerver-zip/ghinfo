# Commit Handoff

## Objective

Complete live HTTP integration coverage for every planned endpoint and record
the final MVP audit state.

## Files changed

- `tests/api_test.cpp`
- `docs/TESTING.md`
- `dev/PLAN.md`
- `dev/COMMIT.md`

## Acceptance criteria

- A local `ApiServer` is started and queried over HTTP.
- Health, readiness, meta, summary, repositories, repository detail, issues,
  pulls, runs, jobs, and activity routes are exercised.
- Repository/status filtering and `400`/`404` behavior are exercised.
- Successful route responses, normalized fields, and readiness-independent
  snapshot serving are verified without GitHub or a PAT.
- The integration test cleans up the listener even when an assertion fails.

## Validation

- `./scripts/validate.sh` — passed.
- `cmake --build --preset dev --parallel` and `ctest --preset dev
  --output-on-failure` — 39 tests passed.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan --parallel` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  39 tests passed.
- `cmake --preset release && cmake --build --preset release --parallel` —
  passed.
- `actionlint .github/workflows/*.yml` — passed.
- `docker compose config --quiet` — passed with expected warnings for unset
  runtime variables.
- `git diff --check` — passed.

## Compatibility and security

No public schema changed. The test uses loopback and synthetic snapshots only;
it performs no internet access and contains no credentials.

## Deferred

Docker image build and remote GitHub Actions/release verification still require
external Docker/root and GitHub remote/authentication state. The current
checkout has no configured Git remote and `gh` is unauthenticated.
