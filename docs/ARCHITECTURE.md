# Architecture

## Architectural thesis

`ghinfo` is a materialized, in-memory view of selected GitHub repository state.

Consumers query local normalized state. They never trigger GitHub traffic.

```text
                  background path

GitHub REST ──> GitHubClient ──> Poller ──> SnapshotStore
                                             |
                                             | immutable snapshot
                                             v
consumer <──────────── HTTP API <──────── ApiServer
```

## Components

### Config

Reads and validates environment variables once during startup.

Important values:

- PAT;
- explicit repository list or automatic discovery mode;
- polling interval;
- bind/port;
- run history bound;
- job-detail expansion bound.

Configuration is immutable after startup in the MVP.

### GitHubClient

Owns GitHub REST transport semantics:

- authentication;
- pinned API version;
- User-Agent;
- timeouts;
- per-path ETag cache and conditional requests;
- JSON parsing;
- pagination;
- ETag conditional requests;
- rate-limit headers;
- error mapping.

It returns normalized source/domain values or explicit errors. It never exposes the PAT.

In automatic discovery mode it lists accessible repositories through GitHub's
`/user/repos` endpoint, follows bounded Link-header pagination, and validates
each returned `full_name` before repository resources are fetched.

Successful responses with an ETag are cached by request path. A later
`304 Not Modified` reuses the cached body while preserving the current
response headers, so rate-limit metadata can advance without discarding the
last usable source state.

### Poller

Runs independently of incoming API traffic.

The refresh loop:

```text
select configured repositories or discover accessible repositories
  |
build complete candidate snapshot
  |
publish atomically
  |
stoppable interval wait
```

The first refresh runs immediately after startup; later refreshes wait for the
configured interval. HTTP request handling never participates in this loop.

Transient GitHub failure preserves last-known-good data. Failed attempts use a
bounded exponential delay; GitHub `Retry-After` and rate-limit reset hints take
precedence when available. Stop requests interrupt both the normal interval
and failure backoff waits.

The first successful complete poll makes `/readyz` return ready.

Refresh construction is all-or-nothing across the selected or discovered
repositories: if discovery, any repository request, or any normalization step
fails, the candidate is discarded and the prior snapshot remains published.

### SnapshotStore

Owns the latest immutable `Snapshot` and the mutable operational poll state.

Readers obtain a `std::shared_ptr<const Snapshot>`.

Writers publish a complete new snapshot rather than mutating shared vectors in place.

This keeps read paths cheap and prevents consumers from observing half-refreshed state.
Poll state is updated independently, so a failed refresh can mark the current
snapshot stale without replacing its data or generation.

### ApiServer

Serves JSON from `SnapshotStore`.

Handlers must not know the PAT and must never invoke `GitHubClient`.

### Prioritized activity projection

The current `/v1/activity` response exposes objective resource groups and a
prioritized, bounded list of attention items derived while the complete
snapshot is being built. HTTP handlers only slice the immutable list for
`?limit=N`; reads do not consume items and do not maintain per-consumer state.

The projection uses domain fields already normalized by ghinfo: `title` for
issues and pull requests, `name` for workflow runs and jobs, stable IDs, and
UTC timestamps. Failed workflow runs and jobs are classified relative to
`Snapshot::generated_at`: active jobs and workflow runs are `critical`, recent
failures up to 7 days are `high` with `recent_failure`, failures over 7 and up
to 30 days are `normal` with `stale_failure`, and older failures are excluded
from the projection. Active work therefore sorts ahead of failures even when
the failure has a newer timestamp. It exposes `critical`, `high`, and `normal`
priority bands, stable signals, and deterministic
recency/repository/kind/ID ordering instead of a consumer-facing opaque score.
Failed runs and failed jobs are distinct items.

The HTTP limit view balances three categories—jobs/workflows, open pull
requests, and open issues—using equal rounds, redistributing missing category
slots and filling remainders by global priority. This selection is computed
from the immutable item vector for each read; it is not a queue and has no
consumer state. The first three items avoid a failed run/job duplicate for the
same repository and workflow run when another candidate exists; the duplicate
is deterministically deferred beyond the protected top three. “New since last
observation” is deferred until first-seen state has an explicit
lifetime/persistence policy.

When a complete snapshot has no open pull requests, the collector performs a
bounded fallback collection of up to 3 recently updated closed pull requests
per repository. These records are stored separately and contribute only to
`Snapshot::activity_items` with normal priority and the
`recent_closed_pull_request` signal. The `/v1/pulls` resource, grouped
`activity.pullRequests` field, repository resource, and summary continue to
describe open pull requests only. Any open pull request suppresses the closed
fallback for that snapshot.

The projection is built once for each complete candidate snapshot and stored in
`Snapshot::activity_items`. An HTTP request validates `limit` and the optional
activity category, performs the deterministic in-memory selection, and copies
the selected values; it never invokes GitHub or mutates the snapshot. Omitting
the category preserves the balanced jobs/workflows, pull requests, and issues
view. A category filter selects only its domain items while retaining the
projection's eligibility, priority, recency, and workflow incident
deduplication rules.

## Domain model

The public model is intentionally smaller than GitHub's REST payloads.

Core entities:

- `Repository`
- `Issue`
- `PullRequest`
- `WorkflowRun`
- `WorkflowJob`
- `Snapshot`

Normalization protects consumers from upstream payload churn and avoids making ghinfo a transparent proxy.

## Poll scope

MVP intent:

- all open issues for each configured or discovered repository;
- all open pull requests;
- up to 3 recently updated closed pull requests per repository only when the
  complete snapshot has no open pull requests, for the activity fallback;
- bounded recent workflow runs;
- jobs for the configured recent workflow-run detail window, plus active runs
  outside that window.

`GHINFO_RUN_HISTORY` controls the workflow runs retained in the snapshot.
`GHINFO_JOB_RUN_HISTORY` independently controls how many of the newest runs
per repository have their jobs fetched. This bounded expansion also discovers
failed jobs inside workflows whose overall conclusion is success, skipped, or
neutral. Active runs are always expanded so current work is not missed.

Exact policies should stay configurable only when real usage requires it.

## Concurrency

Initial target:

- HTTP server managed by `cpp-httplib`;
- one background `std::jthread` poller;
- immutable snapshots exchanged via a small synchronization boundary.

The main thread supervises the HTTP listener and converts `SIGINT`/`SIGTERM`
into an orderly server stop; the poller `std::jthread` then stops through its
stop token.

No worker pool, coroutine runtime, or task scheduler is planned for the MVP.

## Persistence

None in the MVP.

Restart behavior:

```text
start
  -> empty store
  -> /healthz = 200
  -> /readyz = 503
  -> first successful poll
  -> publish snapshot
  -> /readyz = 200
```

Optional atomic snapshot persistence is post-MVP.

## Error semantics

A failed refresh must not erase good state.

The store tracks:

- last poll attempt;
- last successful poll;
- stale status;
- consecutive failures and a safe error category;
- the next retry time;
- current snapshot generation.

Detailed transport errors stay in logs/diagnostic metadata and should not leak
secrets. A successful complete refresh publishes a new generation and resets
the failure state; a failed refresh leaves the last-known-good snapshot intact.

## Decisions intentionally deferred

- self-API authentication;
- snapshot persistence;
- GitHub App auth;
- write operations;
- webhooks;
- GraphQL;
- metrics exporter;
- event streaming.
