---
name: github-rest
description: Implement GitHub REST access with authentication, pinned API version, pagination, conditional requests, rate-limit awareness, and normalized domain output.
---

# GitHub REST

## Invariants

- The PAT is read only by server-side config.
- Send a stable User-Agent.
- Pin the GitHub API version in one transport location.
- Use bounded connect/request timeouts.
- Support pagination where endpoint semantics require it.
- Store and reuse ETags.
- On `304 Not Modified`, reuse cached normalized/raw source state.
- Read rate-limit headers from responses.
- Respect `Retry-After`/reset behavior for throttling.
- Distinguish transport failure, HTTP failure, malformed JSON, and semantic validation failure.
- Never pass raw GitHub objects directly to the public API.

## MVP endpoints to integrate

- issues;
- pull requests;
- Actions workflow runs;
- workflow run jobs.

Remember that GitHub's issues endpoint may include pull requests; filter them from normalized issues.
