FROM debian:bookworm-slim AS builder

WORKDIR /usr/src/app

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    ninja-build \
    clang

COPY . .

RUN --mount=type=cache,target=/usr/src/app/cmake-build-release \
    cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM=ninja \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -G Ninja \
    -S /usr/src/app \
    -B /usr/src/app/cmake-build-release && \
    cmake \
    --build /usr/src/app/cmake-build-release \
    --target slipstream-client slipstream-server \
    -j 18 && \
    cp cmake-build-release/slipstream-client . && \
    cp cmake-build-release/slipstream-server .

FROM gcr.io/distroless/base-debian12

WORKDIR /usr/src/app

COPY ./certs/ ./certs/

ENV PATH=/usr/src/app/:$PATH

COPY --from=builder --chmod=755 /usr/src/app/slipstream-client ./client
COPY --from=builder --chmod=755 /usr/src/app/slipstream-server ./server

LABEL org.opencontainers.image.source https://github.com/EndPositive/slipstream

ENTRYPOINT []
