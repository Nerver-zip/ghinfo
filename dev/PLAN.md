# Active Plan

## Current milestone

**MVP-016 — CI/release v0.1.0**

Target commit:

```text
ci: prepare v0.1.0 release
```

## Goal

Make CI exercise the sanitizer preset and provide auditable, tag-triggered
release automation for the v0.1.0 MVP.

## Acceptance criteria

- Existing GCC/Clang build, test, format, and diff checks remain enabled.
- A dedicated CI job builds and runs the ASan/UBSan preset.
- A tag-triggered workflow creates GitHub release notes using the runner token.
- Existing container workflow publishes GHCR images for version tags.
- Release documentation lists validation, Docker verification, tag, and push
  steps without embedding credentials.
- No remote release is published from this local implementation session.

## Non-goals

- automatic GitHub release from every branch;
- release binaries attached to GitHub;
- credentials in repository files;
- changing the public v1 API.

## Expected files

- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`
- `docs/RELEASE.md`
- `README.md`
- `docs/ROADMAP.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Required skills

- `.agents/skills/ci-cd/SKILL.md`
- `.agents/skills/docker/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan
LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure
actionlint .github/workflows/*.yml
docker compose config --quiet
git diff --check
```
