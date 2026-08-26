# Commit Handoff

## Objective

Fetch normalized job details only for relevant retained workflow runs for
MVP-008.

## Files changed

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- Queued/running and non-successful completed runs fetch their jobs.
- Successful, skipped, and neutral completed runs make no jobs request.
- Jobs use the run-specific endpoint with bounded Link pagination.
- Job IDs/run IDs, status/conclusion, optional timestamps, repository, name,
  and browser URL are normalized into `WorkflowJob`.
- Tests use the checked-in jobs fixture, two local pages, and verify the
  irrelevant-run no-request rule.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 23 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

No public HTTP schema changed. Job retrieval is restricted to selected runs;
raw responses and the PAT remain private.

## Deferred

Snapshot construction, polling, retries/backoff, and public data endpoints
remain assigned to later roadmap milestones.
