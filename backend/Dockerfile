FROM debian:bookworm-slim AS builder

# Устанавливаем инструменты сборки и Ninja для более предсказуемой сборки
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Используем генератор Ninja и параллельную сборку
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANONIM_BUILD_TESTS=OFF \
    && cmake --build build --parallel

FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/anonim_server /app/anonim_server

EXPOSE 8090
CMD ["/app/anonim_server", "8090"]
