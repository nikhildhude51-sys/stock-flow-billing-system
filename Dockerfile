# =============================================================================
# Stage 1: Build Environment
# =============================================================================
FROM debian:bookworm-slim AS builder

# Install C++ compiler, CMake, and build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    g++ \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy CMake configuration, headers, and source files
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/

# Build optimized Release binary with -j2 (prevents memory spikes on cloud builders)
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release . \
    && cmake --build build --target server -j2

# =============================================================================
# Stage 2: Minimal Runtime Environment
# =============================================================================
FROM debian:bookworm-slim AS runner

# Install runtime C++ libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled server binary from builder
COPY --from=builder /build/build/server /app/server

# Copy frontend static assets (HTML, CSS, JavaScript)
COPY public/ /app/public/

# Create runtime data directory with write permissions for persistent JSON storage
RUN mkdir -p /app/data && chmod 777 /app/data

# Set default host and port (Render dynamically overrides PORT at runtime)
ENV HOST=0.0.0.0
ENV PORT=10000

EXPOSE 10000

# Launch Stock Flow Crow backend server
CMD ["/app/server"]
