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

## perf(widget): add lightweight activity items response

The full `/v1/activity` response remains compatible with its grouped arrays.
The additive `/v1/activity/items` endpoint applies the existing limit and
category selection rules but returns only common metadata and `activity.items`.
The ready-to-import Kustom preset now points its four WebGet flows to this
smaller response, keeping the existing global and formulas intact while
reducing repeated JSON parsing on Android.

API tests cover compact payload shape, category filtering, generation, stale
metadata, and HTTP routing. The `.kwgt` archive was rebuilt and verified with
`unzip -t`; extracted content comparison confirms that the four activity URLs
changed and the initial example global was reduced to the compact items shape.
Dev build and 70 tests pass, and the API schema change is additive.
Device-side Kustom timing remains the final verification boundary.

## feat(widget): add interactive Kustom clipboard setup

Add a credential-free Python setup wizard that personalizes the checked-in
native Kustom template with the ghinfo URL and a bounded refresh interval. It
generates a complete `##KUSTOMCLIP##` component, a layout-only clip, and a
customized Premium `.kwgt` under ignored `dist/`. An optional snapshot probe
seeds the initial `ghinfo` global, and the complete clip is copied to the
desktop clipboard when a supported utility is available.

Preserve the existing layout, formulas, Flows, touch actions, and
`FiraCodeNerdFontMono.ttf` reference. Track a default complete clip and
document free clipboard import, command-line options, and font behavior. Add
deterministic Python tests for URL/interval validation, endpoint/cron
customization, marker format, ZIP/font integrity, and secret absence; include
them in `scripts/validate.sh`.

Validation: `./scripts/validate.sh` passes 70 C++ tests and 6 Python tests;
`gitleaks git --redact --no-banner` finds no leaks; `git diff --check` passes.
The public API/schema is unchanged. No GitHub credential is requested or
embedded. Native KWGT clipboard import remains a device-side verification
boundary.

Co-authored-by: Codex <noreply@openai.com>

## docs(readme): refresh project guide and widget preview

Rewrite the root README around the current service contract: local and Compose
quick starts, environment configuration, HTTP routes, prioritized activity,
Kustom Premium/Free setup, security boundaries, development validation, and
troubleshooting. Keep links to the detailed architecture, API, security,
testing, release, roadmap, and widget documents as the source of truth.

Replace the README preview with the corrected Lawnchair capture from the real
widget, including responsive issue-title truncation and the bottom action row.
The public API/schema and runtime behavior are unchanged.

Validation: `./scripts/validate.sh` passes 70 C++ tests and 7 Python tests;
`git diff --check` passes; Gitleaks finds no secrets; local README links and
the PNG decode successfully.

## docs(i18n): translate user-facing setup and widget text

Translate the Kustom setup wizard's prompts, help text, status messages, and
validation errors to English. Align the manual Kustom formulas and generated
`.clip`/`.kwgt` fallback text with the English CLI so every user-facing
workflow uses one language. The API, snapshot schema, and runtime behavior are
unchanged.

Validation: `./scripts/validate.sh` passes 70 C++ tests and 7 Python tests;
`git diff --check` passes; Gitleaks finds no secrets; repository text and
generated preset scans contain no Portuguese user-facing strings.

Co-authored-by: Codex <noreply@openai.com>
