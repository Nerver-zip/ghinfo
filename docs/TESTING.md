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
- timeout/error mapping.
- maximum-width 64-bit identifiers and nullable upstream fields;

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
- status codes/content type;
- summary/resource contracts.

### Golden API tests

Keep representative stable JSON documents under `tests/golden/`.

If a golden changes, reviewers should be able to immediately see public schema drift.

## Sanitizers

Run ASan/UBSan for concurrency/lifetime/network parsing changes.
