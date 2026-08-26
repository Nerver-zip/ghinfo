# API Conventions

- Public data endpoints live under `/v1`.
- Liveness/readiness endpoints are unversioned: `/healthz`, `/readyz`.
- JSON payloads include `schemaVersion` where they represent public domain data.
- Fields use lower camelCase.
- Timestamps are UTC ISO-8601 strings.
- Public enum strings are lowercase snake_case.
- GitHub REST payloads must be normalized before exposure.
- Expose browser-facing GitHub URLs, not raw API URLs, where a public link is needed.
- Do not emit consumer/presentation fields.
- Additive fields are allowed in v1; removals/renames/semantic changes require v2.
- Errors must not expose secrets, internal headers, or raw authorization failures.
