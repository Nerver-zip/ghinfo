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

## fix(poller): bound transport recovery and expose safe failure diagnostics

Transport failures now back off for at most 60 seconds instead of 900;
HTTP/payload failures and rate-limit hints retain their existing policy.
Poll logs include UTC time, HTTP status or libcurl code/reason, consecutive
failures, retry delay, and successful recovery. Raw exception messages and
upstream content are excluded. The public v1 JSON schema is unchanged.

Regression coverage checks transport error codes, safe diagnostic output,
the transport retry ceiling, and actual HTTP snapshot reads while an
upstream request is blocked and after it fails. `./scripts/validate.sh`
passes all 69 tests, formatting, and whitespace checks. The ASan/UBSan preset
also passes all 69 tests without sanitizer overrides. Gitleaks finds no
secrets in the patch. See `dev/PLAN.md` for live incident evidence and the
remaining production/phone verification boundary.
