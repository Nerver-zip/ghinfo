# Commit Handoff

## Objective

Discover all repositories accessible to the authenticated GitHub user when
`GHINFO_REPOSITORIES=auto`, while preserving explicit repository lists.

## Files changed

- `include/ghinfo/config.hpp`
- `include/ghinfo/github_client.hpp`
- `src/config.cpp`
- `src/github_client.cpp`
- `src/snapshot_builder.cpp`
- `tests/config_test.cpp`
- `tests/github_client_test.cpp`
- `tests/snapshot_builder_test.cpp`

## Acceptance criteria

- `auto` is an explicit repository-selection mode; manual lists remain valid.
- `/user/repos` requests use bounded pages and GitHub Link-header pagination.
- Discovered `full_name` values are validated and duplicates are rejected.
- Discovery is part of snapshot construction, so refresh failure preserves the
  last-known-good snapshot through the existing poller policy.
- Positive, malformed, duplicate, and snapshot-integration cases are covered.

## Validation

- `cmake --build --preset dev --parallel` and `ctest --preset dev
  --output-on-failure` — 45 tests passed.
- `git diff --check` — passed.

## Compatibility and security

No public HTTP schema changed. The new request only reads repository metadata;
the PAT remains server-side and tests use loopback synthetic payloads.

## Deferred

Full sanitizer/release validation follows the focused implementation commit.
