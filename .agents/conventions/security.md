# Security Conventions

- GitHub PATs are process-local secrets.
- Never echo secrets in errors, logs, tests, fixtures, Docker layers, or CI artifacts.
- `.env` is local-only and ignored.
- Docker runtime must be non-root.
- Fine-grained PATs should use least privilege.
- Network calls require timeouts.
- Validate repository identifiers before interpolation into URLs.
- API authentication for ghinfo itself is explicitly post-MVP.
