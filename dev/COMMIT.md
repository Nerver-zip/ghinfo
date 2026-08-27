# Commit Handoff

## Objective

Document the planned prioritized activity projection and its lightweight
read-only contract.

## Files changed

- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/API.md`
- `docs/TESTING.md`
- `docs/ROADMAP.md`
- `dev/PLAN.md`
- `dev/COMMIT.md`

## Acceptance criteria

- The next milestone is explicitly post-MVP and requires an ADR.
- The API proposal uses additive `activity.items` and bounded `?limit=N`.
- Priority, recency, titles/names, failed jobs, and deterministic ties are
  specified without introducing queue acknowledgement semantics.
- Current public HTTP behavior remains unchanged.

## Validation

- `git diff --check` — passed.

## Compatibility and security

No public HTTP schema changed; this commit documents a future additive
extension only. No credentials or runtime behavior were changed.

## Deferred

Runtime tests are not required for this documentation-only change; the prior
45-test implementation baseline remains the reference. Docker build and
remote GitHub verification remain external gates.
