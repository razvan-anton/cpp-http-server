# Stage 1: Build environment
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN make clean && make -j$(nproc)

# Stage 2: Runtime image (Use standard ubuntu:24.04)
FROM ubuntu:24.04

WORKDIR /app

# Copy binary from builder stage
COPY --from=builder /app/my_server .

# Copy static assets
COPY --from=builder /app/public ./public

EXPOSE 8080

CMD ["./my_server"]