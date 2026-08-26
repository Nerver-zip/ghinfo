# HTTP API

Status: MVP v1 contract. All endpoints below are read-only and serve the last
complete in-memory snapshot.

## General rules

- JSON responses.
- Public data endpoints use `/v1`.
- Domain payloads include `schemaVersion: 1`.
- Timestamps are UTC ISO-8601.
- Enums serialize as stable lowercase snake_case strings.
- GitHub payloads are normalized.
- No consumer-specific presentation fields.

## `GET /healthz`

Process liveness only.

Response:

```json
{
  "status": "ok"
}
```

Must stay healthy even if GitHub is temporarily unavailable.

## `GET /readyz`

Readiness means a usable snapshot has been published.

Before first successful poll:

```http
503 Service Unavailable
```

```json
{
  "ready": false
}
```

After:

```json
{
  "ready": true
}
```

A stale last-known-good snapshot may still be considered ready.

## `GET /v1/meta`

Always returns `200`. Before the first successful poll it reports
`snapshotAvailable: false`; the `poll` object is still available for safe
operational state. Once a snapshot exists, generation, timestamp, and rate
limit metadata are included.

Example:

```json
{
  "schemaVersion": 1,
  "service": "ghinfo",
  "version": "0.1.0",
  "snapshotAvailable": true,
  "generation": 7,
  "generatedAt": "2026-08-26T20:45:31Z",
  "rateLimit": {
    "limit": 5000,
    "remaining": 4999,
    "resetAt": "2026-08-26T21:00:00Z"
  },
  "poll": {
    "lastAttempt": "2026-08-26T20:45:31Z",
    "lastSuccessful": "2026-08-26T20:45:31Z",
    "stale": false,
    "consecutiveFailures": 0,
    "lastErrorKind": null,
    "nextRetryAt": null
  }
}
```

Future additive fields may include uptime, generation, last poll metadata, and GitHub rate-limit status.

## Planned MVP endpoints

### `GET /v1/summary`

Aggregated counts/state.

Returns `503` with `{"schemaVersion":1,"error":"snapshot_unavailable"}`
until the first complete poll. A stale last-known-good snapshot remains
available with `stale: true`.

Target example:

```json
{
  "schemaVersion": 1,
  "generation": 7,
  "generatedAt": "2026-08-26T20:45:31Z",
  "stale": false,
  "repositories": { "total": 8 },
  "issues": { "open": 14 },
  "pullRequests": { "open": 4, "draft": 1 },
  "actions": {
    "queued": 0,
    "running": 2,
    "failed": 1
  }
}
```

### `GET /v1/repos`

Normalized configured repositories. The response contains `schemaVersion`,
`generation`, `generatedAt`, and a `repositories` array.

### `GET /v1/repos/{owner}/{repo}`

One repository plus currently retained related state in `issues`,
`pullRequests`, `workflowRuns`, and `jobs` arrays. An unknown repository is
`404` with error `repository_not_found`.

### `GET /v1/issues`

Open normalized issues.

Planned filter:

```text
?repo=owner/name
```

### `GET /v1/pulls`

Open normalized pull requests.

### `GET /v1/runs`

Bounded recent workflow runs.

Planned filters:

```text
?repo=owner/name
?status=in_progress
?conclusion=failure
```

Supported `status` values are `queued`, `in_progress`, `completed`, and
`unknown`. Supported `conclusion` values are `success`, `failure`,
`cancelled`, `skipped`, `timed_out`, `neutral`, `action_required`, and
`unknown`. Invalid filters return `400`.

### `GET /v1/jobs`

Relevant jobs from retained workflow runs.

Issues, pulls, runs, and jobs support the `repo=owner/name` filter. Resource
arrays use normalized camelCase fields, explicit lowercase snake_case enum
strings, and nullable conclusion/timestamp fields where GitHub may omit data.

All data endpoints return `503 snapshot_unavailable` before the first complete
poll. They never trigger GitHub requests.

### `GET /v1/activity`

Consumer-neutral "things currently worth inspecting", built only from objective state:

- running jobs;
- failed runs;
- open pull requests;
- recent/open issues.

This endpoint must not invent priority/confidence/display decisions.

## Versioning policy

Allowed within v1:

- additive fields;
- additive endpoints;
- additive enum values only when consumers are expected to tolerate unknown values.

Requires v2:

- field removal;
- field rename;
- type change;
- meaning change;
- incompatible enum semantics.

Golden API tests should detect accidental contract drift.
