#!/bin/bash

###############################################################################
# Build script for GameNetworkingSockets on macOS
#
# Requirements:
# - CMake >= 3.10
# - Ninja build system
# - clang / clang++
# - Homebrew installed (https://brew.sh)
# - Required libraries installed:
#
#   1. OpenSSL 3:
#      brew install openssl@3
#
#   2. libsodium:
#      brew install libsodium
#
#   3. Protobuf v2.6.1 (it is a working version you can use later versions aswell but it might get error, must be built manually):
#      - Download: https://github.com/protocolbuffers/protobuf/releases/tag/v2.6.1
#      - Extract and place in /usr/local
#      - Then run:
#          ./configure
#          make
#          sudo make install
#
# This script builds the GameNetworkingSockets project for macOS using Ninja.
###############################################################################

# Exit immediately on error
set -e

# === Define locations and settings ===
BUILD_DIR="buildMacOS"
SOURCE_DIR="$(pwd)"

# Clean and create build directory
echo "[1/4] Cleaning and creating: $BUILD_DIR"
rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake configuration
echo "[2/4] Starting CMake configuration..."
cmake .. \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_STANDARD=11 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DUSE_CRYPTO=OpenSSL \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/openssl@3;/opt/homebrew/opt/libsodium;/usr/local" \
  -DCMAKE_INCLUDE_PATH="/opt/homebrew/include;/usr/local/include" \
  -DCMAKE_LIBRARY_PATH="/opt/homebrew/lib;/usr/local/lib"

# Start build with Ninja
echo "[3/4] Starting build with Ninja..."
ninja

# Verify output
if [[ -f bin/libGameNetworkingSockets.dylib ]]; then
  echo "[4/4] ✅ Build successful: libGameNetworkingSockets.dylib found"
else
  echo "[4/4] ❌ Build failed: Output library not found"
  exit 1
fi
