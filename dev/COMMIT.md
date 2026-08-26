# Commit Handoff

## Objective

Add conditional requests to the GitHub transport for MVP-004.

## Files changed

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `docs/ARCHITECTURE.md`
- `dev/PLAN.md`

## Acceptance criteria

- Successful ETagged responses are cached by complete request path.
- Cached ETags are sent as `If-None-Match` on later requests.
- `304 Not Modified` returns the cached body with status 304.
- Current 304 headers override cached headers while missing cached headers
  remain available.
- A 304 without a cached response is an explicit HTTP error.
- Cache state is synchronized and isolated per client/path.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 16 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

No public HTTP schema changed. The cache contains response bodies and headers
only; the PAT is never cached or included in diagnostics.

## Deferred

Pagination, resource parsing, polling, retries/backoff, and public data
endpoints remain assigned to later roadmap milestones.
