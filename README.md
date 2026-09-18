<div align="center">

# ghinfo

**A small, self-hosted GitHub status service for widgets, scripts, and internal tools.**

[![CI](https://github.com/Nerver-zip/ghinfo/actions/workflows/ci.yml/badge.svg)](https://github.com/Nerver-zip/ghinfo/actions/workflows/ci.yml)
[![Container](https://github.com/Nerver-zip/ghinfo/actions/workflows/docker.yml/badge.svg)](https://github.com/Nerver-zip/ghinfo/actions/workflows/docker.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![Docker](https://img.shields.io/badge/Docker-supported-2496ED?logo=docker&logoColor=white)](https://www.docker.com/)

[Quick start](#quick-start) · [HTTP API](#http-api) · [Kustom widget](#kustom-widget) · [Development](#development)

</div>

<p align="center">
  <img src="assets/kustom-widget.png" alt="ghinfo Kustom widget on an Android home screen" width="760">
</p>

`ghinfo` polls GitHub's REST API in the background, normalizes repository and
Actions state, and serves a stable read-only JSON snapshot over HTTP. A
consumer can be a Kustom widget, a shell script, a dashboard, or any other
HTTP client—the GitHub credential stays inside the service.

> [!IMPORTANT]
> `ghinfo` is intentionally a read-only information service. It does not
> write to GitHub, store data on disk, receive webhooks, or make GitHub calls
> from HTTP request handlers.

## Highlights

- C++23 single-process daemon with a small runtime footprint.
- Background polling independent from consumer traffic.
- Explicit repository lists or automatic discovery of accessible repositories.
- Normalized, versioned JSON for repositories, issues, pull requests, workflow
  runs, jobs, and prioritized activity.
- Immutable snapshots and last-known-good data during transient failures.
- Conditional requests, pagination, rate-limit hints, bounded backoff, and
  safe failure diagnostics.
- Non-root Docker image with runtime-only secrets.
- Complete Kustom widget artifacts, including a Free-compatible clipboard
  setup wizard.

## How it works

```text
GitHub REST API
       │ authenticated background polling
       ▼
┌──────────────────────────────────┐
│              ghinfo              │
│  GitHubClient → Poller           │
│                    │             │
│                    ▼             │
│          immutable snapshot      │
│                    │             │
│                    ▼             │
│            read-only HTTP API    │
└──────────────────────────────────┘
       │
       ▼
 widgets · scripts · dashboards
```

The poller builds a complete candidate snapshot before publishing it. Readers
obtain the current immutable snapshot, so an HTTP request is local and cheap;
it never waits on GitHub or consumes activity items.

## Quick start

### Prerequisites

- CMake 3.25 or newer
- A C++23 compiler (GCC 13+ or Clang 17+ recommended)
- libcurl development headers
- Git and Ninja (recommended)

For Debian/Ubuntu:

```bash
sudo apt-get install -y build-essential cmake ninja-build \
  libcurl4-openssl-dev clang-format
```

For Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake ninja curl clang
```

### Build and run locally

```bash
git clone https://github.com/Nerver-zip/ghinfo.git
cd ghinfo

cmake --preset dev
cmake --build --preset dev

cp .env.example .env
$EDITOR .env
set -a
source .env
set +a

# Optional for a local-only listener; keep 0.0.0.0 when a phone/LAN client needs access.
export GHINFO_BIND=127.0.0.1

./build/dev/ghinfo
```

The binary listens on `127.0.0.1:8080` when no bind/port overrides are set. The
example `.env` uses `0.0.0.0` so the same values work with Compose; choose the
bind address that matches your deployment. The first poll runs in the
background; `/readyz` becomes ready after the first complete snapshot.

Check the service from another terminal:

```bash
curl -fsS http://127.0.0.1:8080/healthz
curl -i http://127.0.0.1:8080/readyz
curl -fsS 'http://127.0.0.1:8080/v1/activity/items?limit=3'
```

### Run with Docker Compose

Compose reads `.env` automatically and publishes port `8080` by default:

```bash
cp .env.example .env
$EDITOR .env
docker compose up --build -d
curl -fsS http://127.0.0.1:8080/readyz
docker compose logs -f ghinfo
```

Stop the service with `docker compose down`. To publish a different host port,
set `GHINFO_HOST_PORT` in `.env`.

> [!WARNING]
> Setting `GHINFO_BIND=0.0.0.0` makes the API reachable from the network. The
> MVP has no client authentication; expose it only on a trusted LAN or behind
> a reverse proxy/network access layer.

## Configuration

The process reads configuration once at startup. `.env` is a convenient local
file, but the application receives values through the environment.

| Variable | Required | Default | Description |
| --- |:---:|:---:| --- |
| `GHINFO_GITHUB_TOKEN` | yes | — | Fine-grained GitHub PAT with read access to Metadata, Issues, Pull requests, and Actions. |
| `GHINFO_REPOSITORIES` | yes | — | `auto` to discover accessible repositories, or comma-separated `owner/name` values. |
| `GHINFO_POLL_INTERVAL_SECONDS` | no | `60` | Normal refresh interval, from 5 to 3600 seconds. |
| `GHINFO_BIND` | no | `127.0.0.1` | HTTP bind address. |
| `GHINFO_PORT` | no | `8080` | HTTP port, from 1 to 65535. |
| `GHINFO_LOG_LEVEL` | no | `info` | `trace`, `debug`, `info`, `warn`, or `error`. |
| `GHINFO_RUN_HISTORY` | no | `20` | Recent workflow runs retained per repository, from 1 to 100. |
| `GHINFO_JOB_RUN_HISTORY` | no | `10` | Recent runs whose jobs are expanded, from 1 to 100; active runs are also expanded. |
| `GHINFO_HOST_PORT` | no | `8080` | Compose-only host port mapped to container port 8080. |

With `GHINFO_REPOSITORIES=auto`, discovery is authenticated by the server's
PAT and runs as part of each complete refresh. The token is never returned to
clients, written into widget artifacts, or included in logs.

> [!TIP]
> Create a fine-grained PAT restricted to the repositories you want to observe
> and grant read-only access to **Metadata**, **Issues**, **Pull requests**,
> and **Actions**. Keep it in the server environment; never paste it into a
> widget or send it in a client request.

## HTTP API

All public endpoints are read-only and versioned under `/v1`. Data endpoints
serve the last complete in-memory snapshot. Before the first successful poll,
they return `503` with `{"schemaVersion":1,"error":"snapshot_unavailable"}`.

| Endpoint | Purpose |
| --- | --- |
| `GET /healthz` | Process liveness; remains `200` during GitHub outages. |
| `GET /readyz` | Snapshot readiness; `503` until a snapshot exists. A stale snapshot remains readable. |
| `GET /v1/meta` | Schema version, generation, poll state, and rate-limit metadata. |
| `GET /v1/summary` | Repository, issue, pull-request, and Actions counts. |
| `GET /v1/repos` | Configured or discovered repositories. |
| `GET /v1/repos/{owner}/{repo}` | One repository and its retained related resources. |
| `GET /v1/issues` | Open issues; optional `?repo=owner/name`. |
| `GET /v1/pulls` | Open pull requests; optional `?repo=owner/name`. |
| `GET /v1/runs` | Bounded workflow runs; supports repository, status, and conclusion filters. |
| `GET /v1/jobs` | Jobs in the bounded expansion window; supports `?repo=owner/name`. |
| `GET /v1/activity` | Compatibility grouped arrays plus the ordered activity projection. |
| `GET /v1/activity/items` | Compact ordered activity projection for small or frequently-rendered clients. |

Resource arrays use normalized camelCase fields, UTC ISO-8601 timestamps, and
explicit lowercase `snake_case` enum values. Invalid filters return `400`;
unknown repositories return `404`.

### Activity projection

The activity projection is calculated when a complete snapshot is built. It is
not a queue and has no per-consumer state.

```bash
curl -fsS 'http://127.0.0.1:8080/v1/activity/items?limit=3'
curl -fsS 'http://127.0.0.1:8080/v1/activity/items?category=issues&limit=3'
```

- `limit` defaults to `20` and accepts `1` through `100`.
- `category` is optional: `workflows`, `pull_requests`, or `issues`.
- Active runs/jobs are `critical`; recent failures and open issues/pull
  requests are `high`; stale failures, completed runs, and recent closed
  issues/pull requests are `normal`.
- Failures older than 30 days are omitted from `activity.items`.
- Each category is filled independently: open issues/pull requests and active
  workflows appear first, then recent closed records fill any remaining slots
  up to three. An open item therefore does not hide lower-priority items in
  the same category.
- `/v1/activity/items` keeps `schemaVersion`, `generation`, `generatedAt`,
  `stale`, and `activity.items`, while omitting compatibility grouped arrays.

The full contract, resource fields, filters, ordering, and compatibility rules
are documented in [`docs/API.md`](docs/API.md).

## Kustom widget

The repository ships a ready-to-import Premium preset and a Free-compatible
clipboard workflow. The widget uses the compact `/v1/activity/items` endpoint,
four activity Flows, responsive text truncation, and four rectangular action
buttons anchored at the bottom. Its frame and controls adapt to the available
widget width and height; the background remains aligned to the widget's
top-left and covers the full cell so filtered views do not separate the
content from the action row.

### Premium import

Use [`assets/ghinfo-kustom-widget.kwgt`](assets/ghinfo-kustom-widget.kwgt),
import it in KWGT, and change the WebGet URLs if the service is not reachable
at the configured host. The package includes the required
`FiraCodeNerdFontMono.ttf` glyph font. For Free/clipboard imports, download the
standalone [`FiraCodeNerdFontMono.ttf`](assets/fonts/FiraCodeNerdFontMono.ttf)
from this repository, copy it to the phone, and import/select it in KWGT.

### KWGT Free setup

Run the interactive wizard from the repository root:

```bash
python3 setup.py
```

Or configure it non-interactively:

```bash
python3 setup.py \
  --url http://<ghinfo-host>:8080 \
  --refresh-minutes 5
```

The wizard validates the URL and interval, optionally probes the compact
activity endpoint to seed the initial snapshot, and writes ignored artifacts
under `dist/`:

- `ghinfo-kustom-widget.clip` — complete `##KUSTOMCLIP##` component;
- `ghinfo-kustom-widget-loose.clip` — layout-only modules for advanced use;
- `ghinfo-kustom-widget.kwgt` — personalized Premium package.

When available, the complete clip is copied to the clipboard using
`termux-clipboard-set`, `wl-copy`, `xclip`, `xsel`, or `pbcopy`. On Android,
add a blank `4x2` KWGT widget, open **Add → Komponent**, then back out once so
KWGT offers **Paste Komponent from Clipboard**. Paste the complete clip and
save it. Inspect Flows inside the imported **ghinfo Activity Widget**
Komponent; the root widget is not the Flow owner.

> [!IMPORTANT]
> The complete `.clip` references Nerd Font glyphs. Download the repository's
> [`FiraCodeNerdFontMono.ttf`](assets/fonts/FiraCodeNerdFontMono.ttf), copy it
> to the phone, and import/select it in a Free setup before pasting the clip.
> Alternatively, replace the glyphs with plain text. The `.kwgt` package
> already bundles the font.

Use [`Kustom/README.md`](Kustom/README.md) for the full phone sequence,
manual formulas, endpoint checks, and troubleshooting. Do not import the
`*-loose.clip` file when you need the Flows and touch actions.

## Development

The default test suite is hermetic: it uses fake/local HTTP responses and does
not require a GitHub token or live GitHub access.

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Run the canonical quality gate before submitting a change:

```bash
./scripts/validate.sh
```

For lifetime- or concurrency-sensitive changes, also run:

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

Release and container checks are documented in
[`docs/RELEASE.md`](docs/RELEASE.md). The Docker image is multi-stage, runs as
the unprivileged `ghinfo` user, exposes `/healthz` as its healthcheck, and
receives the PAT only when the container starts.

## Project layout

```text
include/ghinfo/     public C++ domain and service interfaces
src/                GitHub transport, polling, snapshots, and HTTP server
tests/              unit, integration, golden, and failure-mode tests
scripts/            validation and Kustom setup tooling
Kustom/             widget instructions and manual formula examples
assets/             importable widget artifacts and preview image
docs/               API, architecture, security, testing, release, and ADRs
```

## Further documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — components, snapshot
  publication, polling, and failure semantics.
- [`docs/API.md`](docs/API.md) — complete v1 JSON contract and activity rules.
- [`docs/SECURITY.md`](docs/SECURITY.md) — threat model, PAT handling, and
  exposure guidance.
- [`docs/TESTING.md`](docs/TESTING.md) — test layers and sanitizer policy.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — MVP milestones and post-MVP scope.
- [`dev/PLAN.md`](dev/PLAN.md) — current engineering state and external gates.

## Troubleshooting

**`/readyz` returns `503`.** The process is alive, but no complete snapshot
has been published yet. Check the startup log, token permissions, repository
scope, and `/v1/meta`.

**The widget shows stale data.** A stale snapshot is intentionally preserved
when GitHub is unavailable. Inspect `poll.stale`, `consecutiveFailures`, and
`nextRetryAt` in `/v1/meta`; the next successful poll will advance the
generation.

**The widget does not respond or has no Flows.** Confirm that the phone can
reach `/v1/activity/items?limit=3`, import the complete `.clip` rather than
`*-loose.clip`, and open the **ghinfo Activity Widget** Komponent to inspect
its Flows. Re-run `python3 setup.py` after changing the service URL.
