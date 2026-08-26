# Commit Handoff

## Objective

Fetch and normalize a bounded recent workflow-run history for MVP-007.

## Files changed

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- Actions workflow runs are requested with `per_page` equal to the configured
  1..100 history limit.
- Returned runs are bounded to that limit.
- Status and conclusion values use explicit enums, with unknown upstream values
  mapped to `unknown` and null conclusions preserved as absent.
- IDs, repository, name, branch, SHA, event, timestamps, and browser URL are
  normalized into `WorkflowRun`.
- Tests use the checked-in runs fixture and a local HTTP response.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 22 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

No public HTTP schema changed. The history bound prevents unbounded source
state; raw payloads and the PAT remain private to the client.

## Deferred

Workflow jobs, polling, retries/backoff, and public data endpoints remain
assigned to later roadmap milestones.
