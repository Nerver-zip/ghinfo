# Commit Handoff

## Objective

Configure ccache for C++ CI jobs and the Docker builder without changing the
runtime image or application data model.

## Files changed

- `.github/workflows/ci.yml`
- `.github/workflows/docker.yml`
- `Dockerfile`
- `docs/DEVELOPMENT.md`
- `dev/PLAN.md`
- `dev/COMMIT.md`

## Acceptance criteria

- GCC, Clang, and ASan CI jobs install and restore separate ccache archives.
- CMake receives compiler-launcher settings explicitly in each C++ job.
- The container build has a BuildKit ccache mount and GitHub Actions layer
  cache configuration.
- The release-only workflow remains unchanged because it has no compile step.
- Runtime behavior, image user, secrets, and public API remain unchanged.

## Validation

- `git diff --check` — passed.
- `actionlint .github/workflows/*.yml` — pending after workflow changes.

## Compatibility and security

No public HTTP schema changed. ccache is a build-only accelerator and is not
copied into the runtime image.

## Deferred

Run the canonical test/build gate and Docker build validation after this
workflow change. Remote cache hit rates require a GitHub Actions run.
