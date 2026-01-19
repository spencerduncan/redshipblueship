#!/bin/bash
set -e

echo "=== RedShipBlueShip Dev Container Setup ==="

# Initialize submodules
echo "Initializing git submodules..."
git submodule update --init --recursive

# Verify build works
echo "Verifying build configuration..."
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

echo "Building combo module tests..."
cmake --build build --target combo_tests -j$(nproc)

echo "Running unit tests..."
ctest --test-dir build --output-on-failure

echo ""
echo "=== Setup Complete ==="
echo "Dev container is ready. You can now:"
echo "  - Build: cmake --build build -j\$(nproc)"
echo "  - Test:  ctest --test-dir build"
echo "  - Full:  cmake --build build && ctest --test-dir build"
