# Commit Handoff

## Objective

Fetch and normalize open pull requests for MVP-006.

## Files changed

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- The pull-request endpoint is requested with `state=open` and bounded pages.
- GitHub `Link: rel="next"` pagination is followed with a safety bound.
- Pull request IDs/numbers, repository, title, author, draft, head/base refs,
  timestamps, and browser URL are normalized into `PullRequest`.
- Invalid payload shapes remain explicit non-transport errors.
- Tests use the checked-in pull fixture and local two-page responses.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 20 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

No public HTTP schema changed. Only normalized pull-request values leave the
GitHub client; the PAT and raw responses remain private.

## Deferred

Workflow resources, polling, retries/backoff, and public data endpoints remain
assigned to later roadmap milestones.
