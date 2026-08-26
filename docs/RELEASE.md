# Release process

The MVP release is `v0.1.0`.

Before tagging:

```bash
./scripts/validate.sh
cmake --preset release
cmake --build --preset release --parallel
actionlint .github/workflows/*.yml
docker compose config --quiet
docker build --tag ghinfo:local .
git diff --check
```

The Docker build must be run on a host with an active Docker daemon. The
runtime image must remain non-root and receive `GHINFO_GITHUB_TOKEN` only at
container start.

Create and publish the tag from a clean, validated checkout:

```bash
git tag -a v0.1.0 -m "ghinfo v0.1.0"
git push origin main v0.1.0
```

The tag triggers the container workflow, which publishes versioned image tags
to GHCR, and the release workflow, which creates GitHub release notes. The
release workflow does not contain credentials; it uses the GitHub Actions
token supplied by the runner.
