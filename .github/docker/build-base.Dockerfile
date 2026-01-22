# Base image for CI builds with all dependencies pre-installed
# This image is used to speed up CI by avoiding repeated dependency compilation
#
# Build versions:
#   - Ubuntu 22.04 (base)
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

# Install apt dependencies
# Note: We install libtinyxml2-dev initially but will replace it with a newer version
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    git \
    wget \
    # From apt-deps.txt
    libusb-dev \
    libusb-1.0-0-dev \
    libsdl2-dev \
    libsdl2-net-dev \
    libpng-dev \
    libglew-dev \
    nlohmann-json3-dev \
    libtinyxml2-dev \
    libspdlog-dev \
    ninja-build \
    libogg-dev \
    libopus-dev \
    opus-tools \
    libopusfile-dev \
    libvorbis-dev \
    libespeak-ng-dev \
    # Additional dependencies for libzip
    zlib1g-dev \
    libbz2-dev \
    liblzma-dev \
    libzstd-dev \
    # Tools
    ccache \
    && rm -rf /var/lib/apt/lists/*

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

# Update library cache
RUN ldconfig

# Clean up
WORKDIR /
RUN rm -rf /tmp/deps

# Set up ccache paths
ENV PATH="/usr/lib/ccache:${PATH}"

# Default working directory for builds
WORKDIR /workspace
