# ADR-0004: Category-filtered activity views

- Status: Accepted
- Date: 2026-08-30
- Scope: Additive `/v1/activity` query behavior

## Context

The diversified activity view is useful for a general dashboard, but a
consumer with separate workflow, pull request, and issue controls needs a
bounded list from one category at a time. Asking that consumer to fetch a
larger mixed list and filter it locally would duplicate projection rules and
make the result fragile. The service must remain consumer-agnostic and must
not turn the activity projection into a notification queue.

## Decision

Add the optional `category` query parameter to `GET /v1/activity` with these
stable values:

- `workflows`: failed and running workflow runs and jobs;
- `pull_requests`: open pull requests;
- `issues`: open issues.

The filter is applied to the already-built immutable activity projection after
failure-age eligibility. Category results preserve the existing priority,
effective timestamp, repository, kind, and ID ordering. Workflow results also
use the existing top-three failed-run/failed-job incident deduplication when a
safe alternative exists.

When `category` is omitted, the existing diversified selection remains
unchanged. Grouped activity arrays and the JSON envelope are returned exactly
as before. Empty or unsupported values fail with HTTP 400 and
`invalid_category`.

The HTTP handler only parses the query and selects from the published
snapshot. It does not call GitHub, mutate state, acknowledge items, or add
consumer-specific fields.

## Consequences

Consumers can request independent bounded lists without reproducing the
priority and aging logic. The service performs only an in-memory scan of the
already-materialized items, so polling cost, GitHub request count, memory
ownership, and snapshot semantics do not change.

The category filter is additive within v1. The no-category behavior remains
the compatibility path for existing clients. A category can return fewer
items than `limit` when that category has fewer eligible candidates.

## Rejected alternatives

- Adding Kustom-specific endpoints or presentation fields would violate the
  consumer-agnostic API boundary.
- Making the default endpoint a workflow-only list would silently change
  existing consumers that rely on diversity.
- Fetching a mixed `limit=9` list and filtering in each consumer would spread
  API semantics into clients and make bounded category results unreliable.
- Adding a queue, acknowledgements, or persistence is unrelated to this
  read-only snapshot query and remains deferred.

## Verification

Deterministic unit and HTTP tests cover valid categories, invalid categories,
empty category results, category isolation, ordering, grouped-field
compatibility, and the unchanged no-category selection.
