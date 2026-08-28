# ADR-0003: Bounded recent workflow-job expansion

- Status: Accepted
- Date: 2026-08-28
- Scope: GitHub Actions collection and additive activity behavior

## Context

The workflow-runs endpoint returns a bounded history, but fetching jobs for
every retained run creates unnecessary GitHub requests and can miss the useful
distinction between a failed job and the overall workflow conclusion. A job can
fail inside a workflow whose conclusion is `success`, `skipped`, or `neutral`.

The activity view needs recent job failures without turning the service into a
historical job archive or adding persistence.

## Decision

Keep `GHINFO_RUN_HISTORY` as the per-repository limit for workflow runs exposed
by `/v1/runs`. Add `GHINFO_JOB_RUN_HISTORY`, defaulting to `10` and accepting
values from `1` through `100`, as the per-repository count of newest workflow
runs whose jobs are expanded.

The collector orders retained runs by `createdAt` descending, with numeric ID
descending as a deterministic tie-breaker, and expands jobs for the first
`GHINFO_JOB_RUN_HISTORY` runs. Queued and in-progress runs are always expanded,
even when they fall outside that recent window. Completed runs outside the
window do not incur job requests.

The job endpoint is queried for every selected run, regardless of the parent
workflow conclusion. This preserves detection of failed jobs in successful,
skipped, and neutral workflows. `/v1/runs` remains the complete configured run
history; `/v1/jobs` and job-derived activity represent the bounded expanded
window.

## Consequences

The normal poll performs at most one paginated job collection per selected
recent or active workflow run per repository. The service remains in-memory,
read-only, and consumer-agnostic. A failed job older than the expansion window
is not available through `/v1/jobs` or `/v1/activity`, but its parent workflow
run remains available through `/v1/runs` while retained there.

The separate bound makes request cost explicit and keeps old workflow failures
from dominating the current photograph. Increasing the bound is an operator
choice with a corresponding GitHub API cost.
