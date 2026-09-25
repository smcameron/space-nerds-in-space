FROM debian:bookworm-slim

LABEL description="Docker image to have Space Nerds In Space ready to play"

# Configure noninteractive apt
ENV DEBIAN_FRONTEND=noninteractive

# Dependencies for build scripts
RUN apt-get update && apt-get install -y --no-install-recommends \
    git ca-certificates build-essential curl make bash \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user and prepare workspace
RUN useradd -m snis && mkdir -p /app && chown -R snis:snis /app
USER snis
WORKDIR /app

# Clone repository
RUN git clone --depth 1 https://github.com/smcameron/space-nerds-in-space.git . 

# Install dependencies
USER root
# Fix install script by removing sudo (not installed but unnecessary since root) and replace libcurl-dev with a package name compatible with this distro
RUN sed -e 's/\bsudo\b//g' -e 's/libcurl-dev/libcurl4-openssl-dev/g' ./util/install_dependencies > ./util/install_dependencies_nosudo_fix \
    && chmod +x ./util/install_dependencies_nosudo_fix
RUN apt-get update && \
    yes | ./util/install_dependencies_nosudo_fix && \
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
USER snis

# Build the game and assets
RUN mkdir -p ~/.local/share/space-nerds-in-space/ \
    && make -j$(nproc) \
    && make models -j$(nproc)

# Set default command
CMD ["./bin/snis_client"]