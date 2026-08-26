# Commit Handoff

## Objective

Harden the production container runtime and process shutdown for MVP-015.

## Files changed

- `src/main.cpp`
- `Dockerfile`
- `docs/ARCHITECTURE.md`
- `docs/SECURITY.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- Release build compiles with tests disabled.
- The daemon handles `SIGINT`/`SIGTERM`, stops the HTTP listener, and joins
  the poller cleanly.
- Runtime remains non-root, declares `SIGTERM`, and Compose retains no-new-
  privileges and capability dropping.
- Healthcheck and deployment documentation match `/healthz` and the runtime
  configuration.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 38 tests passed.
- `cmake --preset release && cmake --build --preset release --parallel` —
  passed.
- `./build/release/ghinfo --version` — returned `ghinfo 0.1.0`.
- `actionlint .github/workflows/*.yml` — passed.
- `docker compose config --quiet` — passed with expected warnings for unset
  runtime variables.
- Process smoke test with `SIGTERM` — exited 0.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.
- `docker build --tag ghinfo:mvp015 .` — not executed: no Docker daemon or
  `/var/run/docker.sock` is available in this environment.

## Compatibility and security

No public API schema changed. Shutdown handling improves deployment behavior;
the container still receives GitHub credentials only through runtime
environment variables and runs without root privileges.

## Deferred

The image build must be repeated on a host with Docker enabled before release
sign-off. CI/release documentation and final MVP audit remain.
