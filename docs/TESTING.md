# Testing Strategy

## Principles

The most valuable tests protect contracts and failure behavior.

No default test should require:

- internet access;
- a real PAT;
- GitHub availability.

## Test layers

### Configuration tests

Cover:

- required token;
- repository parsing;
- invalid repository identifiers;
- integer bounds;
- port bounds;
- log level validation.

### Domain/serialization tests

Cover:

- explicit enum string conversion;
- 64-bit IDs;
- deterministic JSON;
- optional fields.

### GitHub transport tests

Once transport lands, use local/fake responses for:

- required headers;
- pagination;
- ETag capture;
- `If-None-Match`;
- `304`;
- `403`/`429`;
- `Retry-After`;
- rate-limit reset hints;
- malformed JSON;
- timeout/error mapping;
- maximum-width 64-bit identifiers, nullable upstream fields, invalid UTC
  timestamps, and mismatched repository identities;
- automatic repository discovery, Link-header pagination, malformed names,
  and duplicate repository detection;

### Poller tests

Cover:

- first snapshot publication;
- last-known-good preservation;
- partial refresh failure;
- stale metadata;
- clean stop;
- bounded exponential backoff and rate-limit hint precedence;
- backoff reset after success;
- all-or-nothing behavior when one configured repository fails.

### HTTP tests

Cover:

- health;
- readiness;
- meta;
- summary, resources, and activity;
- filters plus `400`/`404`/`503` status behavior;
- content type and normalized JSON contracts over a local HTTP server.

### Prioritized activity tests

Before implementation, add deterministic coverage for:

- default and bounded `limit` behavior;
- invalid, zero, and over-limit values returning `400`;
- failed runs and failed jobs receiving the intended activity kinds;
- issue/PR `title` and run/job `name` surviving projection;
- recency and stable tie-break ordering;
- repeated reads returning the same items without acknowledgement or mutation;
- stale snapshots remaining readable with the same ordering rules.

The current suite covers these cases with unit, HTTP, snapshot-integration,
and golden-contract tests.

### Golden API tests

Keep representative stable JSON documents under `tests/golden/`.

If a golden changes, reviewers should be able to immediately see public schema drift.

## Sanitizers

Run ASan/UBSan for concurrency/lifetime/network parsing changes.
