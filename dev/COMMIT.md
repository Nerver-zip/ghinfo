# Commit Handoff

## Objective

Add repository-history secret scanning with Gitleaks and keep the handoff as a
record of implementation and evidence rather than an operator checklist.

## Files changed

- `.github/workflows/ci.yml`
- `docs/SECURITY.md`
- `dev/COMMIT.md`

## Acceptance criteria

- A dedicated Gitleaks job runs on push, pull request, and manual CI runs.
- The scan checks complete Git history through a full repository checkout.
- The scan job has read-only repository permissions and does not publish
  comments or artifacts.
- The application PAT is not exposed to the scan job.
- No runtime behavior, public API, or Docker image content changes.

## Validation

- `actionlint .github/workflows/*.yml` — passed.
- `./scripts/validate.sh` — passed; 45/45 tests passed.
- `gitleaks git --redact --no-banner` — passed; 31 commits scanned and no
  leaks found.
- `git diff --check` — passed.
- Docker build — not executed in this environment because the Docker daemon
  socket is unavailable.

## Compatibility and security

No public HTTP schema changed. Gitleaks is CI-only and uses the workflow's
read-only `GITHUB_TOKEN`; the application PAT remains outside the scan job.

## External state

Remote CI execution and remote tag publication are outside this local change.
