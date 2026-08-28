# Security

## Threat model for MVP

The primary sensitive asset is the GitHub PAT.

The intended trust boundary is:

```text
consumer -> ghinfo HTTP API

            [PAT exists only here]
                    |
                  ghinfo
                    |
                 GitHub
```

## PAT handling

- Read from `GHINFO_GITHUB_TOKEN`.
- Never return it from an endpoint.
- Never log it.
- Never persist it.
- Never bake it into a Docker image.
- Never commit `.env`.
- Do not include real tokens in fixtures/tests.

Use a fine-grained PAT restricted to the repositories ghinfo should observe
with the minimum read permissions necessary for Metadata, Issues, Pull
requests, and Actions. Automatic discovery only returns repositories visible
to that token; it does not expand the token's authority.

## HTTP exposure

Default bind is `127.0.0.1`.

Docker Compose overrides this to `0.0.0.0` because container port publication requires it.

The MVP does not implement its own client authentication. If exposing beyond a trusted host/LAN, place ghinfo behind a trusted network layer or reverse proxy.

API-token authentication for ghinfo itself is post-MVP.

## Input boundaries

Treat as untrusted:

- all environment variables;
- repository identifiers;
- GitHub response bodies;
- GitHub headers;
- request paths/query parameters.

Validate before use.

`GHINFO_RUN_HISTORY` and `GHINFO_JOB_RUN_HISTORY` are bounded configuration
inputs. The latter limits workflow-job API expansion to recent or active runs,
which also limits the amount of untrusted upstream data held in the snapshot.

## Logging

Logs may include:

- repository full name;
- endpoint category;
- HTTP status;
- retry/backoff state;
- poll generation/duration.

Logs must not include:

- Authorization header;
- token fragments;
- `.env`;
- complete sensitive HTTP dumps.

## Docker

The runtime image uses a non-root user, drops all Linux capabilities in
Compose, disables privilege escalation, and declares `SIGTERM` as its stop
signal.

Secrets are supplied only at runtime.

## Secret scanning

The CI workflow runs Gitleaks against the complete repository history on every
push, pull request, and manual workflow dispatch. The scan job has read-only
repository permissions, does not publish pull-request comments or artifacts,
and does not receive the application PAT. A finding fails the CI workflow.

## Reporting vulnerabilities

Until a formal security policy exists, use a private communication channel with the repository owner rather than opening an issue containing secrets.
