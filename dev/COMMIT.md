# Commit Handoff

## Objective

Fetch and normalize open issues for MVP-005.

## Files changed

- `CMakeLists.txt`
- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `dev/PLAN.md`

## Acceptance criteria

- The issues endpoint is requested with `state=open`, 100-item pages, and the
  configured repository.
- GitHub `Link: rel="next"` pagination is followed with a safety bound.
- Pull requests containing the `pull_request` field are omitted from issues.
- Issue IDs/numbers, repository, title, author, labels, timestamps, and browser
  URL are normalized into `Issue`.
- Malformed JSON is reported as `malformed_json`; invalid payload shape is
  reported as `semantic`.
- Tests use the checked-in fixture and local HTTP responses across two pages.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 19 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

No public HTTP endpoint changed. Upstream payloads are parsed into domain
values; raw GitHub JSON and the PAT do not cross the client boundary.

## Deferred

Pull requests, workflow resources, polling, retries/backoff, and public data
endpoints remain assigned to later roadmap milestones.
