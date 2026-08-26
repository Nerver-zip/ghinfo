# Active Plan

## Current milestone

**MVP-003 — Authenticated GitHub transport**

Target commit:

```text
feat(github): add authenticated REST transport
```

## Goal

Implement the smallest reusable libcurl-based transport required for later GitHub REST resources.

## Acceptance criteria

- `GitHubClient` can execute an authenticated GET against a supplied GitHub REST path.
- The transport always supplies:
  - `Authorization: Bearer <token>`;
  - `Accept: application/vnd.github+json`;
  - pinned `X-GitHub-Api-Version`;
  - stable `User-Agent`.
- Connect and total request timeouts are bounded.
- Response status, body, and selected headers are captured.
- Transport errors are distinct from non-2xx HTTP responses.
- The token is not present in user-facing/loggable error messages.
- Unit tests do not contact GitHub and do not require a real token.

## Non-goals

- pagination;
- ETag caching;
- issues/PRs/runs/jobs;
- polling;
- retries/backoff;
- public data endpoints.

Those belong to later roadmap milestones.

## Expected files

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- possibly a small local HTTP test helper

## Required skills

- `.agents/skills/modern-cpp/SKILL.md`
- `.agents/skills/github-rest/SKILL.md`
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
git diff --check
```
