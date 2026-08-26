---
name: ci-cd
description: Maintain focused GitHub Actions for C++ validation and container builds without turning CI into project complexity.
---

# CI/CD

Required PR/push validation should remain fast:

- configure;
- build;
- CTest;
- format gate.

Container workflow should build the production image. Publishing is only done where explicitly configured.

Do not make expensive static analysis a blocking gate without demonstrated value.
