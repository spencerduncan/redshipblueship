# Base image for CI builds with all dependencies pre-installed
# This image is used to speed up CI by avoiding repeated dependency compilation
#
# Build versions:
#   - Ubuntu 22.04 (base)
#   - CMake 3.26+ (from Kitware APT repo)
#   - SDL2 2.30.3 (with hidapi-libusb support)
#   - SDL2_net 2.2.0
#   - tinyxml2 10.0.0 (with position-independent code)
#   - libzip 1.10.1 (without crypto)
#
FROM ubuntu:22.04

LABEL org.opencontainers.image.source="https://github.com/spencerduncan/redshipblueship"
LABEL org.opencontainers.image.description="Build environment for redshipblueship with pre-compiled dependencies"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Add Kitware APT repository for CMake 3.26+
# Ubuntu 22.04's default cmake is 3.22.x, but we need 3.26+
RUN apt-get update && apt-get install -y ca-certificates gpg wget \
    && wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null \
    && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null \
    && apt-get update \
    && rm -rf /var/lib/apt/lists/*

# Copy apt-deps.txt to use as single source of truth for apt packages
# This file is shared with CI workflows to avoid package list drift
COPY .github/workflows/apt-deps.txt /tmp/apt-deps.txt

# Install apt dependencies
# Note: We install libtinyxml2-dev initially but will replace it with a newer version
# Packages from apt-deps.txt are installed via tr/xargs to keep a single source of truth
RUN apt-get update && apt-get install -y \
    # Build essentials (not in apt-deps.txt)
    build-essential \
    cmake \
    git \
    # Additional dependencies for libzip (not in apt-deps.txt)
    zlib1g-dev \
    libbz2-dev \
    liblzma-dev \
    libzstd-dev \
    # Tools (not in apt-deps.txt)
    ccache \
    curl \
    # Install packages from apt-deps.txt
    && tr ' ' '\n' < /tmp/apt-deps.txt | xargs apt-get install -y \
    && rm -rf /var/lib/apt/lists/* /tmp/apt-deps.txt

# Create build directory for dependencies
WORKDIR /tmp/deps

# Build and install SDL2 2.30.3 with hidapi-libusb support
RUN wget -q https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-2.30.3.tar.gz \
    && tar -xzf SDL2-2.30.3.tar.gz \
    && cd SDL2-2.30.3 \
    && ./configure --enable-hidapi-libusb \
    && make -j$(nproc) \
    && make install \
    && cp -av /usr/local/lib/libSDL* /lib/x86_64-linux-gnu/ \
    && cd .. \
    && rm -rf SDL2-2.30.3 SDL2-2.30.3.tar.gz

# Build and install SDL2_net 2.2.0
RUN wget -q https://www.libsdl.org/projects/SDL_net/release/SDL2_net-2.2.0.tar.gz \
    && tar -xzf SDL2_net-2.2.0.tar.gz \
    && cd SDL2_net-2.2.0 \
    && ./configure \
    && make -j$(nproc) \
    && make install \
    && cp -av /usr/local/lib/libSDL* /lib/x86_64-linux-gnu/ \
    && cd .. \
    && rm -rf SDL2_net-2.2.0 SDL2_net-2.2.0.tar.gz

# Remove system tinyxml2 and build version 10.0.0 with position-independent code
RUN apt-get update && apt-get remove -y libtinyxml2-dev && rm -rf /var/lib/apt/lists/* \
    && wget -q https://github.com/leethomason/tinyxml2/archive/refs/tags/10.0.0.tar.gz \
    && tar -xzf 10.0.0.tar.gz \
    && cd tinyxml2-10.0.0 \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    && make -j$(nproc) \
    && make install \
    && cd ../.. \
    && rm -rf tinyxml2-10.0.0 10.0.0.tar.gz

# Build and install libzip 1.10.1 with crypto disabled
RUN wget -q https://github.com/nih-at/libzip/releases/download/v1.10.1/libzip-1.10.1.tar.gz \
    && tar -xzf libzip-1.10.1.tar.gz \
    && cd libzip-1.10.1 \
    && mkdir build && cd build \
    && cmake .. \
        -DENABLE_COMMONCRYPTO=OFF \
        -DENABLE_GNUTLS=OFF \
        -DENABLE_MBEDTLS=OFF \
        -DENABLE_OPENSSL=OFF \
    && make -j$(nproc) \
    && make install \
    && cp -av /usr/local/lib/libzip* /lib/x86_64-linux-gnu/ \
    && cd ../.. \
    && rm -rf libzip-1.10.1 libzip-1.10.1.tar.gz

# Pre-download gamecontrollerdb.txt for SDL controller mappings
# This avoids needing network access during cmake configure
RUN mkdir -p /opt/redshipblueship \
    && curl -sSfL -o /opt/redshipblueship/gamecontrollerdb.txt \
       https://raw.githubusercontent.com/mdqinc/SDL_GameControllerDB/master/gamecontrollerdb.txt

# Update library cache
RUN ldconfig

# Clean up
WORKDIR /
RUN rm -rf /tmp/deps

# Set up ccache paths
ENV PATH="/usr/lib/ccache:${PATH}"

# Default working directory for builds
WORKDIR /workspace
