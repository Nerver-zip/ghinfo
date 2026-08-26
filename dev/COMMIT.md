# Commit Handoff

## Objective

Prepare CI and tag-triggered release automation for ghinfo v0.1.0.

## Files changed

- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`
- `docs/RELEASE.md`
- `README.md`
- `docs/ROADMAP.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- CI keeps GCC/Clang, tests, formatting, and diff checks and adds an ASan/UBSan
  job.
- A `v*` tag creates a GitHub release with generated notes using the scoped
  Actions token.
- The existing tag-aware container workflow remains responsible for GHCR
  publication.
- Release documentation is reproducible and secret-safe.

## Validation

- `actionlint .github/workflows/*.yml` — passed.
- `docker compose config --quiet` — passed with expected warnings for unset
  runtime variables.
- `./scripts/validate.sh` — pending before commit.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  pending before commit; the local executor requires this LeakSanitizer
  workaround because tests run under ptrace.
- `git diff --check` — pending before commit.
- `docker build --tag ghinfo:local .` — still unavailable because this
  environment has no Docker daemon.

## Compatibility and security

No runtime or public API schema changed. The release workflow requests only
`contents: write`; container publication remains scoped to the existing
workflow permissions. No PAT or Docker credential is stored in the repository.

## Deferred

Remote CI/release execution, GHCR publication, and Docker image verification
remain operator/runner outcomes rather than local claims.
