# Commit Handoff

## Objective

Document automatic repository discovery and its Fine-grained PAT boundary.

## Files changed

- `.env.example`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/API.md`
- `docs/SECURITY.md`
- `docs/TESTING.md`
- `docs/ROADMAP.md`
- `dev/PLAN.md`
- `dev/COMMIT.md`

## Acceptance criteria

- Configuration shows `auto` and the explicit list alternative.
- Documentation states that discovery is limited by the PAT's repository access.
- Architecture, API, testing, roadmap, and active-plan docs describe the mode.
- Existing public HTTP schema remains unchanged.

## Validation

- Post-implementation `ctest --preset dev --output-on-failure` — 45 tests
  passed.
- `git diff --check` — passed.

## Compatibility and security

No public HTTP schema changed. Documentation clarifies that the new read-only
discovery request is constrained by the PAT; no credential is included.

## Deferred

Full sanitizer/release validation was already completed for the same code
before the documentation commit. Docker build and remote GitHub verification
remain external gates.
