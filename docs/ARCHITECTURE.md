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
- selected repositories;
- polling interval;
- bind/port;
- run history bound.

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

Successful responses with an ETag are cached by request path. A later
`304 Not Modified` reuses the cached body while preserving the current
response headers, so rate-limit metadata can advance without discarding the
last usable source state.

### Poller

Runs independently of incoming API traffic.

The refresh loop:

```text
refresh configured repositories
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

Refresh construction is all-or-nothing across configured repositories: if any
repository request or normalization step fails, the candidate is discarded
and the prior snapshot remains published.

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

- all open issues;
- all open pull requests;
- bounded recent workflow runs;
- jobs only for runs useful to current status (queued/running/failed/etc.).

Exact policies should stay configurable only when real usage requires it.

## Concurrency

Initial target:

- HTTP server managed by `cpp-httplib`;
- one background `std::jthread` poller;
- immutable snapshots exchanged via a small synchronization boundary.

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
