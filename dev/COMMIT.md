# Commit Handoff

## Objective

Implement the reusable authenticated GitHub REST GET transport required by
MVP-003.

## Files changed

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`

## Acceptance criteria

- libcurl executes authenticated GET requests against a configured API base URL.
- Requests include Bearer authentication, the GitHub JSON Accept header, the
  pinned API version, and the stable User-Agent.
- Connect and total timeouts are bounded.
- Response status, body, and case-insensitive response headers are captured.
- Transport failures and non-2xx HTTP responses have distinct error kinds.
- HTTP error messages exclude the token and upstream response body.
- Tests use a local HTTP server and never require GitHub or a real PAT.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 15 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

The public HTTP API is unchanged. The PAT is stored only inside
`GitHubClient`, used to construct the Authorization header, and excluded from
errors and tests.

## Deferred

ETag caching, pagination, resource parsing, polling, retries, and public data
endpoints remain assigned to later roadmap milestones.
