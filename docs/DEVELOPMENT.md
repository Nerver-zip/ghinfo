# Development

## Toolchain

- C++23
- CMake 3.25+
- Ninja recommended
- libcurl
- cpp-httplib
- nlohmann/json
- GoogleTest

## Presets

### Dev

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Warnings are treated as errors for project code.

CI configures `ccache` as the C and C++ compiler launcher and restores a
compiler cache per toolchain/job. The Docker builder uses a BuildKit cache
mount for the same purpose. These are build accelerators only; they do not
change runtime behavior or persist application data.

### Release

```bash
cmake --preset release
cmake --build --preset release
```

### ASan/UBSan

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

## Validation

Canonical:

```bash
./scripts/validate.sh
```

Format check:

```bash
./scripts/check-format.sh
```

## Prioritized activity

The `/v1/activity` projection is derived during complete snapshot construction.
Its contract and ordering decisions are recorded in
[`docs/adr/0001-prioritized-activity-projection.md`](adr/0001-prioritized-activity-projection.md),
and the public response is documented in [`docs/API.md`](API.md). Changes to
priority bands, item kinds, ordering, or historical state require an ADR and
corresponding contract tests.

## Dependency policy

Dependencies must have a clear reason.

Current production dependencies:

- libcurl: outbound GitHub HTTPS transport;
- cpp-httplib: inbound HTTP server;
- nlohmann/json: JSON.

Test-only:

- GoogleTest.

Do not add Boost/network runtimes/database libraries while the MVP remains solvable with the current stack.

## Testing

The default suite must be hermetic: no real GitHub token and no live GitHub requests.

See `docs/TESTING.md`.

## Commit workflow

Keep commits aligned with `docs/ROADMAP.md`.

Before commit:

```bash
./scripts/validate.sh
git diff --check
```

Never report validation as passed without running it.
