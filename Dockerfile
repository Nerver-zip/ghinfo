# syntax=docker/dockerfile:1.7

FROM debian:bookworm-slim AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    ccache \
    cmake \
    curl \
    git \
    libcurl4-openssl-dev \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN --mount=type=cache,id=ghinfo-ccache,target=/root/.cache/ccache \
    cmake --preset release \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    && cmake --build --preset release --parallel

FROM debian:bookworm-slim AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --create-home --home-dir /home/ghinfo ghinfo

COPY --from=builder /src/build/release/ghinfo /usr/local/bin/ghinfo

USER ghinfo
WORKDIR /home/ghinfo

STOPSIGNAL SIGTERM

ENV GHINFO_BIND=0.0.0.0
ENV GHINFO_PORT=8080

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD ["curl", "--fail", "--silent", "http://127.0.0.1:8080/healthz"]

ENTRYPOINT ["/usr/local/bin/ghinfo"]
