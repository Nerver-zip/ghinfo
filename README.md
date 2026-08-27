# ghinfo

A tiny, headless, self-hosted GitHub status service.

`ghinfo` polls GitHub in the background, normalizes repository state into a stable domain model, and serves read-only JSON snapshots over HTTP. It is intentionally consumer-agnostic: widgets, dashboards, scripts, and other clients are all just HTTP consumers.

## Goals

- Small C++23 daemon with low idle overhead.
- One process, one container, no database in the MVP.
- GitHub credentials stay on the server.
- Background polling is completely independent from API requests.
- Last-known-good snapshots remain available during GitHub outages.
- Stable, versioned JSON contracts instead of leaking raw GitHub payloads.
- Simple enough to reach a useful MVP in roughly 10–20 focused commits.

## Explicit non-goals for the MVP

- No frontend or Kustom-specific endpoints.
- No webhooks.
- No SQLite, Redis, PostgreSQL, or persistence layer.
- No GraphQL unless REST becomes demonstrably insufficient.
- No GitHub write operations.
- No OAuth, multi-user accounts, or GitHub App installation flow.
- No notifications, analytics history, SSE, or WebSockets.
- No Prometheus dependency.

## Planned architecture

```text
GitHub REST API
      |
      | GHINFO_GITHUB_TOKEN
      v
+-------------------------------+
|            ghinfo             |
|                               |
|  GitHubClient -> Poller       |
|                    |          |
|                    v          |
|               SnapshotStore   |
|                    |          |
|                    v          |
|                HTTP API       |
+-------------------------------+
      |
      | JSON
      v
unknown consumer
```

API handlers only read the latest immutable snapshot. They never synchronously call GitHub.

## Current scaffold status

The scaffold already provides:

- C++23 + target-based CMake;
- pinned `cpp-httplib`, `nlohmann/json`, and GoogleTest dependencies;
- system `libcurl`;
- typed environment configuration;
- normalized domain types and snapshot store;
- GitHub client and poller boundaries ready for implementation;
- `/healthz`, `/readyz`, and `/v1/meta`;
- CTest unit tests;
- Docker + Compose;
- GitHub Actions CI and container workflow;
- formatting, warnings, sanitizer preset, and validation scripts;
- `AGENTS.md` plus focused `.agents` skills, conventions, and prompts;
- architecture, API, security, testing, development, and MVP roadmap docs.

The implementation covers authenticated transport, conditional requests,
normalized GitHub resources, automatic or explicit repository selection,
complete snapshots, a resilient background poller, the full v1 read API,
activity aggregation, runtime hardening, and CI/release automation through
**MVP-017**. The remaining release action is to
run the Docker build on a host with an active daemon, then tag/push `v0.1.0`.
See [`dev/PLAN.md`](dev/PLAN.md) and
[`docs/ROADMAP.md`](docs/ROADMAP.md).

Release instructions are in [`docs/RELEASE.md`](docs/RELEASE.md).

## Quick start

### Requirements

- CMake 3.25+
- C++23 compiler (GCC 13+ or Clang 17+ recommended)
- libcurl development package
- Git
- Docker/Compose only if using containers

Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake ninja curl clang
```

Ubuntu/Debian:

```bash
sudo apt-get install -y build-essential cmake ninja-build libcurl4-openssl-dev clang-format
```

### Build

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Or:

```bash
./scripts/validate.sh
```

### Run

Create a local environment file:

```bash
cp .env.example .env
```

Edit:

```dotenv
GHINFO_GITHUB_TOKEN=github_pat_...
GHINFO_REPOSITORIES=auto
# Or use a comma-separated list: owner/repo-a,owner/repo-b
```

Then:

```bash
cmake --preset dev
cmake --build --preset dev
./build/dev/ghinfo
```

The daemon starts the HTTP API and performs its first GitHub poll in the
background. `/readyz` remains unavailable until that poll publishes a complete
snapshot.

```bash
curl http://127.0.0.1:8080/healthz
curl http://127.0.0.1:8080/readyz
curl http://127.0.0.1:8080/v1/meta
```

### Docker

```bash
docker compose up --build
```

The image runs as a non-root user.

## Configuration

| Variable | Required | Default | Purpose |
|---|---:|---|---|
| `GHINFO_GITHUB_TOKEN` | yes | — | Fine-grained GitHub PAT |
| `GHINFO_REPOSITORIES` | yes | — | `auto` or comma-separated `owner/repo` list |
| `GHINFO_POLL_INTERVAL_SECONDS` | no | `60` | Base polling interval |
| `GHINFO_BIND` | no | `127.0.0.1` | HTTP bind address |
| `GHINFO_PORT` | no | `8080` | HTTP port |
| `GHINFO_LOG_LEVEL` | no | `info` | `trace`, `debug`, `info`, `warn`, `error` |
| `GHINFO_RUN_HISTORY` | no | `20` | Maximum recent workflow runs retained per repo |

For Docker/Compose, set `GHINFO_BIND=0.0.0.0`.

## GitHub token permissions

The intended MVP is read-only. Use a fine-grained PAT restricted to the
repositories that ghinfo should observe, with read access for:

- Metadata
- Issues
- Pull requests
- Actions

Never commit the token. `.env` is ignored.

With `GHINFO_REPOSITORIES=auto`, ghinfo discovers the repositories returned by
GitHub for the authenticated user, including repositories where the token has
the necessary access. The discovery is refreshed on every polling cycle, so
newly accessible repositories are picked up without a restart. A token cannot
discover repositories it was not granted access to.

## Repository layout

```text
.
├── .agents/
├── .github/
├── cmake/
├── dev/
├── docs/
├── include/ghinfo/
├── scripts/
├── src/
├── tests/
├── CMakeLists.txt
├── CMakePresets.json
├── Dockerfile
├── compose.yaml
└── AGENTS.md
```

## Development workflow

Before implementing a milestone:

1. Read `AGENTS.md`.
2. Read `dev/PLAN.md`.
3. Read the relevant `.agents/skills/*/SKILL.md`.
4. Define observable acceptance criteria.
5. Add or update tests first when practical.
6. Keep the change focused.
7. Run `./scripts/validate.sh`.
8. Update the roadmap/plan only with verified state.

Commit messages use Conventional Commits:

```text
feat(github): add conditional requests
test(api): add summary golden response
fix(poller): preserve last good snapshot
```

## License

MIT. See [`LICENSE`](LICENSE).
