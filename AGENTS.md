# Project Instructions for Agents

## Mission

Build `ghinfo` as a tiny, headless, self-hosted C++23 service that polls GitHub REST APIs, normalizes state, and serves read-only JSON snapshots to arbitrary consumers.

The server owns GitHub authentication. Consumers do not receive or provide GitHub credentials.

## Product boundary

`ghinfo` is an information service, not a dashboard and not an automation platform.

The MVP must remain:

- consumer-agnostic;
- read-only;
- REST-based;
- polling-based;
- in-memory;
- single-process;
- simple to operate with Docker.

Do not add consumer-specific fields, UI concepts, Kustom endpoints, persistence, webhook ingestion, GraphQL, GitHub writes, OAuth, a GitHub App, a message broker, or a plugin system unless the roadmap explicitly moves beyond the MVP and the change is justified by an ADR.

## Priority order

1. Correctness of reported GitHub state.
2. Secret safety.
3. Stable API contracts.
4. Resilience to GitHub/API failures.
5. Simplicity and maintainability.
6. Low resource usage.
7. Measured performance.

## Before changing files

1. Read `README.md`.
2. Read `docs/ARCHITECTURE.md`, `docs/API.md`, `docs/SECURITY.md`, and the relevant section of `docs/ROADMAP.md`.
3. Read `dev/PLAN.md` for active work.
4. Read the applicable `.agents/skills/<skill>/SKILL.md`.
5. Inspect existing code and tests. Do not assume an API or capability exists.
6. State observable acceptance criteria before non-trivial implementation.

## Mandatory engineering rules

- Use C++23 and target-based CMake.
- Root namespace is `ghinfo`.
- Prefer value semantics, RAII, strong domain types, and explicit ownership.
- Avoid dynamic polymorphism unless a test seam truly needs it.
- No GitHub request may originate from an HTTP request handler.
- API handlers only read immutable snapshots.
- Polling failures must not destroy the last-known-good snapshot.
- Never log `Authorization`, PATs, `.env` contents, or secrets.
- Treat every GitHub response and environment variable as untrusted input.
- Use bounded timeouts for network operations.
- Honor GitHub rate-limit and conditional-request semantics.
- Prefer additive API changes. Breaking public schema changes require `/v2`.
- Stable enum values are serialized as explicit strings, never ordinal integers.
- IDs use fixed-width integer types.
- Timestamps exposed by the API are UTC ISO-8601.
- Keep PRs and commits small, focused, and reversible.
- Do not disable warnings, tests, sanitizer checks, or format gates to make code pass.
- Do not claim tests/CI/benchmarks passed unless they were actually executed.
- Do not optimize concurrency until profiling demonstrates a need.

## Canonical validation

```bash
./scripts/validate.sh
```

This is the default completion gate.

Use the sanitizer preset for memory/lifetime-sensitive work:

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

## Skills

Use the smallest relevant set:

- `modern-cpp`: general C++ design/implementation.
- `cmake-build`: build system or dependency changes.
- `cpp-testing`: tests and regression work.
- `github-rest`: GitHub API transport, parsing, pagination, ETag, rate limits.
- `polling`: background refresh, snapshots, stale state, backoff.
- `api-contract`: public JSON schema/endpoints.
- `docker`: image/Compose/runtime changes.
- `ci-cd`: GitHub Actions.
- `cpp-code-review`: completion review.

## Completion format for agents

Report:

- summary;
- modified files;
- acceptance criteria satisfied;
- commands executed and outcomes;
- public API/schema impact;
- security implications;
- remaining risks or intentionally deferred work.

## Stop and reconsider when

- a requested feature requires consumer-specific presentation logic;
- a handler would synchronously call GitHub;
- a secret could leave the process;
- a schema change silently breaks consumers;
- a dependency is being added without clear need;
- a database/webhook/GraphQL solution is proposed before the REST polling MVP is proven;
- the last-known-good snapshot would be discarded on transient failure;
- a quality gate needs to be weakened.
