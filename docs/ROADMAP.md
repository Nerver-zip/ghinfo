# Roadmap to MVP

Target: a functional read-only GitHub status service in roughly 16 focused commits.

The scaffold represents the initial project setup and design baseline. Commit numbering below is a recommended execution sequence, not a requirement to preserve exact historical numbers if the implementation evolves.

## MVP definition

The MVP is complete when:

- a container starts from environment-only configuration;
- a fine-grained PAT remains server-side;
- selected repositories are polled independently of consumers;
- open issues and PRs are normalized;
- workflow runs and relevant jobs are normalized;
- conditional requests reduce unnecessary GitHub traffic;
- rate-limit/backoff behavior is safe;
- last-known-good state survives transient GitHub failure;
- consumers can query summary, repo, issue, PR, run, job, and activity JSON;
- the API contains no Kustom/UI-specific behavior;
- CI validates the project.

## Recommended commit sequence

### MVP-001 — Bootstrap C++23 project

**Commit:** `chore: bootstrap C++23 project`

Delivered by this scaffold:

- CMake/CMakePresets;
- strict project warnings;
- source/include/tests layout;
- formatting/tidy/editor config;
- base executable/version.

### MVP-002 — Agent/docs/config baseline

**Commit:** `docs(agents): define project execution rules`

Delivered by this scaffold:

- `AGENTS.md`;
- `.agents` skills/conventions/prompts;
- architecture/API/security/testing/development docs;
- typed environment config;
- Docker/CI skeleton.

### MVP-003 — Authenticated GitHub transport

**Commit:** `feat(github): add authenticated REST transport`

Acceptance:

- libcurl request wrapper;
- Bearer auth;
- stable User-Agent;
- pinned GitHub API version;
- connect/total timeouts;
- error type separating transport and HTTP failure;
- token never appears in errors/logs;
- unit tests use a fake/local HTTP endpoint.

### MVP-004 — Conditional request cache

**Commit:** `feat(github): support conditional requests`

Acceptance:

- capture ETag;
- emit `If-None-Match`;
- reuse cached body/state on `304`;
- capture core rate-limit headers;
- tests for 200 -> 304 lifecycle.

### MVP-005 — Issues

**Commit:** `feat(github): fetch open issues`

Acceptance:

- paginate;
- exclude pull requests returned by the issues endpoint;
- normalize issue fields;
- fixture tests.

### MVP-006 — Pull requests

**Commit:** `feat(github): fetch open pull requests`

Acceptance:

- paginate;
- normalize draft/base/head/author/timestamps/URL;
- fixture tests.

### MVP-007 — Workflow runs

**Commit:** `feat(github): fetch workflow runs`

Acceptance:

- retain bounded recent history;
- normalize status/conclusion/branch/SHA/event;
- fixture tests.

### MVP-008 — Workflow jobs

**Commit:** `feat(github): fetch relevant workflow jobs`

Acceptance:

- fetch jobs only for retained runs that need detail;
- normalize status/conclusion/timestamps;
- avoid unbounded historical growth.

### MVP-009 — Snapshot builder

**Commit:** `feat(poller): build repository snapshots`

Acceptance:

- combine repositories/issues/PRs/runs/jobs;
- complete candidate snapshot before publish;
- generation metadata;
- deterministic ordering where API contracts rely on it.

### MVP-010 — Background poller

**Commit:** `feat(poller): add periodic background refresh`

Acceptance:

- `std::jthread`;
- configurable interval;
- clean stop;
- no GitHub request from HTTP handlers;
- first successful poll makes store ready.

### MVP-011 — Resilience and backoff

**Commit:** `feat(poller): preserve last-known-good state`

Acceptance:

- failure preserves previous snapshot;
- stale metadata;
- bounded exponential backoff;
- reset after success;
- respect rate-limit retry/reset hints.

### MVP-012 — Core read API

**Commit:** `feat(api): expose normalized status resources`

Acceptance:

- `/v1/summary`;
- `/v1/repos`;
- `/v1/repos/{owner}/{repo}`;
- `/v1/issues`;
- `/v1/pulls`;
- `/v1/runs`;
- `/v1/jobs`;
- basic repo/status/conclusion filters;
- golden JSON tests.

Status: delivered by the current implementation and its API golden/resource
tests.

### MVP-013 — Activity view

**Commit:** `feat(api): add aggregated activity endpoint`

Acceptance:

- `/v1/activity`;
- objective grouping only;
- no presentation scoring/priority.

Status: delivered by the current implementation and focused API tests.

### MVP-014 — Hardening

**Commit:** `test: harden GitHub failure and API contracts`

Acceptance coverage:

- pagination;
- malformed upstream payloads;
- timeout;
- 304;
- 403/429;
- stale state;
- partial repo failure policy;
- 64-bit IDs;
- API goldens.

Status: delivered by the current transport, snapshot, and API regression
tests.

### MVP-015 — Production container

**Commit:** `feat(container): add production Docker image`

The scaffold provides an initial image/Compose definition. This milestone finalizes and verifies it against the implemented daemon:

- multi-stage build;
- non-root runtime;
- healthcheck;
- SIGTERM;
- no secret in layers;
- documented Compose flow.

### MVP-016 — CI/release v0.1.0

**Commit:** `ci: prepare v0.1.0 release`

Acceptance:

- C++ CI green;
- container build green;
- GHCR tag workflow;
- README examples reflect actual output;
- `v0.1.0` release.

## Post-MVP candidates

Only after usage demonstrates need:

### v0.2

- richer filtering;
- optional self-API bearer token;
- per-resource polling cadence.

### v0.3

- atomic snapshot file persistence for fast restart.

### v0.4

- GitHub App auth for broader multi-repository installation.

### v0.5

- carefully scoped write actions such as rerunning failed workflows.

Any move into writes changes the security/product boundary and should have an ADR.
