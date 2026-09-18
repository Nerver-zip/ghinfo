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
  "version": "0.4.0",
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

Normalized configured or discovered repositories. The response contains `schemaVersion`,
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

Open normalized pull requests. This endpoint contains only open pull requests;
recent closed pull requests used to fill activity previews are not included.

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

Jobs from the expanded workflow-run detail window. The window includes the
`GHINFO_JOB_RUN_HISTORY` newest runs per repository and any active runs outside
that window. It is independent of `GHINFO_RUN_HISTORY`, which controls the
workflow runs returned by `/v1/runs`.

Issues, pulls, runs, and jobs support the `repo=owner/name` filter. Resource
arrays use normalized camelCase fields, explicit lowercase snake_case enum
strings, and nullable conclusion/timestamp fields where GitHub may omit data.

All data endpoints return `503 snapshot_unavailable` before the first complete
poll. They never trigger GitHub requests.

### `GET /v1/activity`

Consumer-neutral "things currently worth inspecting", built only from objective state:

- running workflow runs and jobs;
- failed runs;
- completed workflow runs;
- open pull requests followed by recent closed pull requests when the preview
  still has room;
- open issues followed by recent closed issues when the preview still has room.

Response shape:

```json
{
  "schemaVersion": 1,
  "generation": 7,
  "generatedAt": "2026-08-26T20:45:31Z",
  "stale": false,
  "activity": {
    "runningRuns": [],
    "runningJobs": [],
    "failedRuns": [],
    "pullRequests": [],
    "issues": []
  }
}
```

The grouped arrays are preserved for compatibility. `runningRuns` is an
additive group for queued or in-progress workflow runs; the existing
`runningJobs`, `failedRuns`, `pullRequests`, and `issues` groups retain their
previous meanings. It returns `503 snapshot_unavailable` before the first
complete poll.

### Prioritized activity projection

The activity response includes an additive ordered `items` array. Consumers
may request a bounded number of items with `GET /v1/activity?limit=N`.

The omitted `limit` defaults to `20`; valid values are `1` through `100`.
Empty, non-numeric, zero, negative, overflowing, and over-limit values return
`400` with `invalid_limit`. The existing grouped arrays remain unchanged.

An optional `category` parameter returns the same ordered projection restricted
to one activity category:

```text
GET /v1/activity?category=workflows&limit=3
GET /v1/activity?category=pull_requests&limit=3
GET /v1/activity?category=issues&limit=3
```

`workflows` includes `failed_run`, `failed_job`, `running_run`, `running_job`,
and `completed_run` items. `pull_requests` includes `pull_request` items, and
`issues` includes only `issue` items. Category filtering occurs after the
temporal eligibility policy, so expired failures remain absent from every
category view. The selected items retain the existing priority, recency, and
deterministic tie-break ordering; workflow views also retain failed
run/failed job incident deduplication for the protected top three when an
alternative exists.

When `category` is omitted, the existing diversified selection is preserved.
Grouped arrays are returned unchanged regardless of the category filter, and
the response envelope remains the same. An empty or unsupported category
returns `400` with `invalid_category`.

### `GET /v1/activity/items`

Lightweight form of the activity projection for consumers that only need the
ordered items. It accepts the same optional `limit` and `category` parameters
and applies the same eligibility, priority, ordering, diversity, and incident
deduplication rules as `/v1/activity`. The response contains the common
`schemaVersion`, `generation`, `generatedAt`, and `stale` fields plus
`activity.items`; it omits `runningRuns`, `runningJobs`, `failedRuns`,
`pullRequests`, and `issues`. Before the first complete poll it returns the
same `503 snapshot_unavailable` response.

This endpoint is additive and does not change the full `/v1/activity`
compatibility response. Its smaller body is suitable for clients that parse
the same JSON value repeatedly during rendering.

Each item contains `kind`, `priority`, `signals`, `repository`, `id`, nullable
`updatedAt`, and `url`. The item kinds are:

- `failed_job`: failed workflow job, `high` priority when recent and `normal`
  when stale, with `runId`, `name`, `status`, and `conclusion`;
- `failed_run`: failed workflow run, `high` priority when recent and `normal`
  when stale, with `name`, `status`, and `conclusion`;
- `running_job`: queued or in-progress job, `critical` priority, with `runId`,
  `name`, and `status`;
- `running_run`: queued or in-progress workflow run, `critical` priority, with
  `name` and `status`;
- `completed_run`: completed workflow run, `normal` priority, with `name`,
  `status`, and `conclusion`;
- `pull_request`: an open pull request (`high` priority) or a recent closed
  pull request (`normal` priority), with `number` and `title`;
- `issue`: an open issue (`high` priority) or a recent closed issue (`normal`
  priority), with `number` and `title`.

Each category preview is filled independently. The collector requests a
bounded page of recent closed records only when the corresponding open count
is below three, then the category projection places open/active records first
and uses recent closed records to fill the remaining slots. Therefore one open
issue or pull request does not suppress recent closed items in that category.
Closed records are exposed only through `activity.items`, with
`recent_closed_pull_request` or `recent_closed_issue` signals. The grouped
`activity.pullRequests` and `activity.issues` fields, `/v1/pulls`, `/v1/issues`,
repository arrays, and summary counts remain open-only.

Failed workflow runs and jobs use a temporal policy relative to the snapshot's
`generatedAt`. A failure updated within the previous 7 days is `high` and
receives the `recent_failure` signal. A failure older than 7 and at most 30
days becomes `normal` and receives `stale_failure`. A failure older than 30
days is omitted from `activity.items`. A future-dated upstream timestamp is
treated as current. If a failure timestamp cannot be evaluated, the failure
is retained at its base urgency without an age signal; normalized GitHub
payloads are expected to contain valid UTC timestamps.

Running jobs and workflow runs are `critical` regardless of age, so active
work always sorts ahead of failures. Open pull requests and issues remain
available with `high` priority. Recent closed records and completed non-failure
workflow runs are `normal`. The resulting priority order is `critical` active
work, `high` recent failures and open records, then `normal` stale failures,
completed runs, and recent closed records. This priority is an explicit
urgency classification; recency still orders items within a priority band.

The complete eligible item set is ordered by priority band, effective
timestamp descending, repository ascending, kind ascending, and stable
numeric ID ascending. Effective timestamps use `updatedAt` for issues, pull
requests, runs, and running jobs; a job uses `completedAt` followed by
`startedAt`. Items without a job timestamp sort after timestamped items in
their priority band. `priority` and `signals` are explicit strings; no opaque
numeric score is exposed.

The `limit` view applies deterministic diversity after eligibility filtering:

- limits 1 and 2 return the best global candidates;
- limits 3 through 5 start with one jobs/workflows, one pull request, and one
  issue when those categories have candidates, then fill remaining slots by
  global order;
- limit 6 starts with two candidates from each category;
- larger limits use `floor(limit / 3)` candidates per category and fill the
  remainder by global order;
- missing categories redistribute their unused slots.

The jobs/workflows category contains failed, running, and completed workflow
runs plus failed and running jobs. Failed runs and failed jobs remain distinct
items. When an alternative
exists, the first three returned items avoid showing both the failed run and a
failed job from the same `repository` and workflow run. If no alternative
exists, both remain available. The returned items follow the deterministic
global ordering, except that a duplicate incident is deferred after the
protected top three when the selected set already contains a safe alternative.

The projection is calculated during complete snapshot construction. Reads
select from immutable snapshot data and never acknowledge, remove, reserve,
or maintain per-consumer state. `/v1/runs`, `/v1/jobs`, and the grouped arrays
are not filtered by activity age or diversity. Workflow runs remain bounded by
`GHINFO_RUN_HISTORY` per repository, while jobs are bounded by the separate
`GHINFO_JOB_RUN_HISTORY` expansion window plus active runs. “All history” means
all history retained or expanded by those configurations, not the complete
history available on GitHub. `firstSeenAt`, event history, and persistence are
deferred.

Illustrative future shape:

```json
{
  "activity": {
    "items": [
      {
        "id": "run:owner/repo:123",
        "kind": "failed_run",
        "priority": "high",
        "signals": ["failed_run", "recent"],
        "repository": "owner/repo",
        "name": "CI",
        "occurredAt": "2026-08-27T01:10:00Z",
        "updatedAt": "2026-08-27T01:12:00Z",
        "url": "https://github.com/owner/repo/actions/runs/123"
      }
    ],
    "total": 1,
    "limit": 10
  }
}
```

“Newly observed since the previous poll” still requires `firstSeenAt` state and
is deferred; creation/update recency is evaluated without persistence.

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
