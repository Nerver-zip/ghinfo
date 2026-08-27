# Active Plan

## Current state

The implementation milestones through MVP-017 are complete in the local
checkout. Final goal closure is waiting only for external verification that
cannot be performed in this environment.

## Final audit criteria

- All planned HTTP routes are exercised over a local server, including filters
  and `400`/`404`/`503` responses.
- Canonical dev and sanitizer validation remain green after the audit fix.
- Every commit has post-commit status, whitespace, and relevant test evidence.
- Docker image build and remote GitHub Actions/release state are verified on a
  host with Docker and a configured GitHub remote.
- Upstream timestamps and repository identity are rejected when they violate
  the normalized domain contract.
- Automatic repository discovery is paginated, validated, and integrated into
  complete snapshot construction.
- No tag or remote release is created without those external gates.

## Current external blockers

- No Git remote is configured in this checkout, and `gh` is unauthenticated.
- Docker CLI exists, but no daemon socket is available; `dockerd` requires
  unavailable root privileges.

## Required next evidence

```bash
docker build --tag ghinfo:local .
git remote -v
gh run list --limit 20
git tag -a v0.1.0 -m "ghinfo v0.1.0"
git push origin main v0.1.0
```

The tag/push commands are intentionally not executed by this local run.

## Next planned milestone after MVP

Design and implement a prioritized activity projection as an additive
extension to `/v1/activity`:

- approve an ADR for priority bands and explainable signals;
- add `?limit=N` with a safe default and maximum;
- derive ordered items during snapshot construction;
- preserve the existing grouped activity fields;
- include issue/PR titles and run/job names;
- represent failed jobs separately from failed workflow runs;
- keep reads non-consuming and consumer-agnostic;
- defer `firstSeenAt` semantics until state lifetime/persistence is decided.

The implementation should remain in-memory and should not add SQLite, a broker,
notifications, or per-consumer acknowledgement state.
