---
name: docker
description: Keep ghinfo containerized as a minimal, non-root, secret-safe single-service deployment.
---

# Docker

## Rules

- Multi-stage build.
- Non-root runtime user.
- No PAT in image layers.
- Use environment variables at runtime.
- `/healthz` is the container healthcheck target.
- Do not use `/readyz` for restart decisions.
- Keep runtime dependencies minimal.
- Gracefully handle SIGTERM.

## Verification

```bash
docker build -t ghinfo:dev .
docker compose config
```
