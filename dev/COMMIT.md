# Commit Handoff

## Objective

Deliver the prioritized activity projection as an additive, in-memory,
consumer-agnostic extension to `/v1/activity`.

## Delivered

- ADR-0001 defines the contract, priority bands, signals, ordering,
  compatibility, and deferred historical state.
- Immutable snapshots contain ordered activity items for failed jobs, failed
  runs, running jobs, pull requests, and issues.
- `limit` defaults to 20, accepts 1 through 100, and rejects invalid values
  with `invalid_limit`.
- Existing grouped activity fields remain available.
- Titles, names, stable IDs, effective timestamps, priorities, and signals are
  exposed without consumer-specific presentation logic.

## Validation

- `./scripts/validate.sh` — passed; 50/50 tests passed.
- ASan/UBSan build and tests with leak detection disabled for the sandbox's
  ptrace-compatible execution — passed; 50/50 tests passed.
- `./scripts/check-format.sh` — passed.
- `git diff --check` — passed.
- `gitleaks git --redact --no-banner` — passed; no leaks found.

## Compatibility and security

The change is additive to the v1 activity schema. Handlers read immutable
snapshot data and never call GitHub. No persistence, acknowledgement state,
consumer credentials, or secret-bearing fields were added.

## External state

Remote CI/release state and Docker image verification remain external. The
local checkout preserves the unrelated `.ai-jail` worktree change.
