# Git Conventions

Use Conventional Commits.

Preferred scopes:

- `config`
- `core`
- `github`
- `poller`
- `api`
- `container`
- `ci`
- `docs`
- `test`

Examples:

```text
feat(github): add conditional requests
fix(poller): preserve last good snapshot
test(api): add summary golden response
docs: define MVP contract
```

One commit should deliver one coherent capability. Avoid unrelated cleanup.
