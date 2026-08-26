# Active Plan

## Current milestone

**MVP-005 — Open issues**

Target commit:

```text
feat(github): fetch open issues
```

## Goal

Fetch all open issues for each configured repository, follow pagination, omit
pull requests returned by GitHub's issues endpoint, and normalize the result
into the public domain model.

## Acceptance criteria

- Requests use the repository issues endpoint with `state=open` and bounded
  pages.
- Pagination follows GitHub's `Link` `rel="next"` response semantics.
- Entries containing `pull_request` are excluded from issues.
- IDs, numbers, title, author, labels, timestamps, and browser URL are
  normalized into `Issue` values.
- Malformed JSON and invalid payload shapes are explicit non-transport errors.
- Tests use the checked-in fixture and a local HTTP server, including at least
  two pages.

## Non-goals

- pull requests, workflow runs, or jobs;
- polling, retries, and backoff;
- public data endpoints.

## Expected files

- `include/ghinfo/config.hpp`
- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `CMakeLists.txt`
- `docs/ARCHITECTURE.md`
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
