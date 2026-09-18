# ADR-0007: Mixed-state activity previews

- Status: Accepted
- Date: 2026-09-18
- Scope: workflow, pull-request, and issue activity projections

## Context

The activity widget requests a small, bounded preview for each category. A
binary fallback policy made one high-priority item suppress all lower-priority
records: one open issue meant no recent closed issues were available, and the
same happened for pull requests. Workflow views also lacked a neutral,
completed record after active and failed work.

## Decision

Build each category independently and apply its limit after collection and
ordering:

- active workflow runs/jobs first, then failed work, then completed runs;
- open pull requests first, then recently updated closed pull requests;
- open issues first, then recently updated closed issues.

The collector requests only the missing closed issue/PR slots when the open
count is below three. Closed issue/PR records remain separate from open
resource arrays and appear only in `activity.items` with explicit
`recent_closed_issue` or `recent_closed_pull_request` signals. Completed runs
use the additive `completed_run` kind and `completed_workflow` signal.

The open-only `/v1/issues`, `/v1/pulls`, grouped activity fields, repository
resources, and summary counts remain unchanged. HTTP handlers continue to read
only immutable snapshots; no GitHub request is made during a read.

## Consequences

Category views remain useful when a category has a mixture of states, while
the existing priority and recency ordering is preserved. The change adds at
most one bounded closed-issue request per selected repository when the issue
preview is short, in addition to the equivalent pull-request request.

## Verification

Client tests verify the closed-issue query and normalization. Snapshot tests
verify that closed issue and pull-request records fill previews even when open
records exist. Activity tests verify ordering for all three mixed-state
categories, and API/golden tests verify the additive `completed_run` kind and
the unchanged open-only resource contracts.
