# HTTP API

Status: scaffold contract. Only `/healthz`, `/readyz`, and `/v1/meta` exist initially.

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

Scaffold response includes service/version and snapshot presence.

Planned shape:

```json
{
  "schemaVersion": 1,
  "service": "ghinfo",
  "version": "0.1.0",
  "snapshotAvailable": false
}
```

Future additive fields may include uptime, generation, last poll metadata, and GitHub rate-limit status.

## Planned MVP endpoints

### `GET /v1/summary`

Aggregated counts/state.

Target example:

```json
{
  "schemaVersion": 1,
  "generatedAt": "2026-08-26T20:45:31Z",
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

Normalized configured repositories.

### `GET /v1/repos/{owner}/{repo}`

One repository plus currently retained related state.

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

### `GET /v1/jobs`

Relevant jobs from retained workflow runs.

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
