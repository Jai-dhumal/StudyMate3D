# Multi-stage Docker build for StudyMate 3D (C++ Backend + WebGL Frontend)
# Stage 1: Build the C++ executable on Linux
FROM gcc:12-bookworm AS builder

WORKDIR /app

# Copy backend source files
COPY src/ /app/src/

# Compile the C++17 multi-threaded server with pthread
RUN g++ -std=c++17 -O2 -Isrc/backend src/backend/main.cpp -o studymate_server -lpthread

# Stage 2: Minimal runtime image
FROM debian:bookworm-slim

# Install curl and ca-certificates for outgoing HTTPS requests (Gemini AI API)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary from builder
COPY --from=builder /app/studymate_server /app/studymate_server

# Copy static frontend and data directories
COPY public/ /app/public/
COPY data/ /app/data/

# Default port for Render / Cloud hosting
ENV PORT=8080

EXPOSE 8080

CMD ["/app/studymate_server"]
