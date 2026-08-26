---
name: api-contract
description: Design stable consumer-agnostic JSON endpoints and guard schema compatibility.
---

# API Contract

## Principles

- Consumer agnostic.
- Domain-oriented, not presentation-oriented.
- JSON only for MVP.
- `/v1` for public data.
- Stable strings for enums.
- UTC ISO-8601 timestamps.
- Browser-facing GitHub URLs.
- No PATs, REST authorization headers, or raw transport details.

## Planned MVP endpoints

- `/healthz`
- `/readyz`
- `/v1/meta`
- `/v1/summary`
- `/v1/repos`
- `/v1/repos/{owner}/{repo}`
- `/v1/issues`
- `/v1/pulls`
- `/v1/runs`
- `/v1/jobs`
- `/v1/activity`

Protect public contracts with golden JSON tests.
