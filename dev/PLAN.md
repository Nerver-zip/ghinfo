# Active Plan

## Current milestone

**MVP-006 — Pull requests**

Target commit:

```text
feat(github): fetch open pull requests
```

## Goal

Fetch all open pull requests for each configured repository, follow pagination,
and normalize the review-relevant state into `PullRequest` values.

## Acceptance criteria

- Requests use the repository pull-request endpoint with `state=open` and
  bounded pages.
- GitHub `Link` `rel="next"` pagination is followed.
- IDs/numbers, title, author, draft, head/base refs, timestamps, and browser URL
  are normalized.
- Invalid JSON and invalid payload shapes are explicit non-transport errors.
- Tests use the checked-in pull-request fixture and local two-page responses.

## Non-goals

- workflow runs or jobs;
- polling, retries, and backoff;
- public data endpoints.

## Expected files

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `dev/COMMIT.md`

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
