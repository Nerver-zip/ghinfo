# Active Plan

## Current milestone

**MVP-015 — Production container**

Target commit:

```text
feat(container): harden production runtime
```

## Goal

Make the existing multi-stage image and Compose deployment match the actual
daemon lifecycle and security boundary.

## Acceptance criteria

- Release build compiles the daemon without tests in the builder stage.
- Runtime image remains non-root, secret-free, and minimal.
- Compose drops capabilities and disables privilege escalation.
- `SIGINT`/`SIGTERM` stop the HTTP listener and poller cleanly.
- Runtime healthcheck targets `/healthz`.
- Static Compose/actionlint checks pass; Docker build is run when a daemon is
  available.

## Non-goals

- orchestration beyond Compose;
- persistent volumes;
- container-side GitHub credentials;
- image optimization without measurement.

## Expected files

- `src/main.cpp`
- `Dockerfile`
- `compose.yaml`
- `docs/ARCHITECTURE.md`
- `docs/SECURITY.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Required skills

- `.agents/skills/modern-cpp/SKILL.md`
- `.agents/skills/docker/SKILL.md`
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
cmake --preset release
cmake --build --preset release
docker compose config --quiet
docker build --tag ghinfo:mvp015 .
actionlint .github/workflows/*.yml
git diff --check
```
