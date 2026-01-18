FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04

# Install all build dependencies (synced with .github/workflows/apt-deps.txt)
RUN apt-get update && apt-get install -y \
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
    libzip-dev \
    clang-tidy \
    && rm -rf /var/lib/apt/lists/*

# Install latest SDL2 (matching CI)
RUN wget https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-2.30.3.tar.gz && \
    tar -xzf SDL2-2.30.3.tar.gz && \
    cd SDL2-2.30.3 && \
    ./configure --enable-hidapi-libusb && \
    make -j$(nproc) && \
    make install && \
    cd .. && \
    rm -rf SDL2-2.30.3 SDL2-2.30.3.tar.gz && \
    cp -av /usr/local/lib/libSDL* /lib/x86_64-linux-gnu/

# Install latest tinyxml2 (matching CI)
RUN apt-get update && apt-get remove -y libtinyxml2-dev && \
    wget https://github.com/leethomason/tinyxml2/archive/refs/tags/10.0.0.tar.gz && \
    tar -xzf 10.0.0.tar.gz && \
    cd tinyxml2-10.0.0 && \
    mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    cd ../.. && \
    rm -rf tinyxml2-10.0.0 10.0.0.tar.gz && \
    rm -rf /var/lib/apt/lists/*
