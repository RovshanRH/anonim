FROM debian:bookworm-slim AS builder

# Устанавливаем инструменты сборки и Ninja для более предсказуемой сборки
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    ninja-build \
    pkg-config \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Используем генератор Ninja и параллельную сборку
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

FROM builder AS test

WORKDIR /src
CMD ["ctest", "--test-dir", "/src/build", "--output-on-failure"]

FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/anonim_server /app/anonim_server

EXPOSE 8090
ENTRYPOINT ["/app/anonim_server"]
CMD ["8090"]
