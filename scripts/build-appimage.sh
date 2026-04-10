#!/bin/bash
#
# MidnightSun AppImage Build Script
# Builds the AppImage for headless game streaming
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
APPIMAGE_DIR="$BUILD_DIR/AppDir"
ARTIFACTS_DIR="$PROJECT_DIR/artifacts"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    log_error "This script must be run on Linux"
    exit 1
fi

# Check for required commands
check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "$1 is not installed"
        return 1
    fi
    return 0
}

log_info "Checking dependencies..."
check_command cmake || exit 1
check_command ninja || exit 1
check_command wget || exit 1
check_command git || exit 1

# Install build dependencies if on Ubuntu/Debian
if command -v apt-get &> /dev/null; then
    log_info "Installing build dependencies..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        libdrm-dev \
        libgl-dev \
        libwayland-dev \
        libx11-xcb-dev \
        libxcb-dri3-dev \
        libxfixes-dev \
        libgtk-3-dev \
        librsvg2-dev \
        libfuse2 \
        libcap-dev \
        libcurl4-openssl-dev \
        libevdev-dev \
        libgbm-dev \
        libminiupnpc-dev \
        libnotify-dev \
        libnuma-dev \
        libopus-dev \
        libpipewire-0.3-dev \
        libpulse-dev \
        libssl-dev \
        libsystemd-dev \
        libudev-dev \
        libx11-dev \
        libxcb-shm0-dev \
        libxcb-xfixes0-dev \
        libxcb1-dev \
        libxrandr-dev \
        libxtst-dev \
        libvulkan-dev \
        libva-dev \
        glslang-tools \
        npm \
        python3-jinja2 \
        python3-setuptools
fi

# Create directories
log_info "Creating build directories..."
mkdir -p "$BUILD_DIR"
mkdir -p "$APPIMAGE_DIR"
mkdir -p "$ARTIFACTS_DIR"

# CMake configure with AppImage flag
log_info "Configuring CMake with AppImage flag..."
cd "$BUILD_DIR"
cmake "$PROJECT_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSUNSHINE_ASSETS_DIR=share/sunshine \
    -DSUNSHINE_EXECUTABLE_PATH=/usr/bin/sunshine \
    -DSUNSHINE_BUILD_APPIMAGE=ON \
    -DSUNSHINE_ENABLE_DRM=ON \
    -DSUNSHINE_ENABLE_PORTAL=ON \
    -DSUNSHINE_ENABLE_WAYLAND=ON \
    -DSUNSHINE_ENABLE_X11=ON

# Build
log_info "Building MidnightSun..."
ninja

# Install to AppDir
log_info "Installing to AppDir..."
DESTDIR="$APPIMAGE_DIR" ninja install

# Copy custom AppRun
log_info "Copying custom AppRun..."
cp -f "$PROJECT_DIR/packaging/AppImage/AppRun" "$APPIMAGE_DIR/"
chmod +x "$APPIMAGE_DIR/AppRun"

# Download linuxdeploy
log_info "Downloading linuxdeploy..."
cd "$BUILD_DIR"
if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
fi

# Download linuxdeploy GTK plugin
if [ ! -f "linuxdeploy-plugin-gtk.sh" ]; then
    wget -q https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh
    chmod +x linuxdeploy-plugin-gtk.sh
fi

# Set GTK version
export DEPLOY_GTK_VERSION=3

# Create AppImage
log_info "Creating AppImage..."
./linuxdeploy-x86_64.AppImage \
    --appdir "$APPIMAGE_DIR" \
    --plugin gtk \
    --executable "$APPIMAGE_DIR/usr/bin/sunshine" \
    --desktop-file "$PROJECT_DIR/packaging/AppImage/com.midnightsun.app.desktop" \
    --output appimage

# Find and rename the AppImage
APPIMAGE_FILE=$(find "$BUILD_DIR" -name "*.AppImage" -type f | head -1)
if [ -n "$APPIMAGE_FILE" ]; then
    mv "$APPIMAGE_FILE" "$ARTIFACTS_DIR/MidnightSun-x86_64.AppImage"
    chmod +x "$ARTIFACTS_DIR/MidnightSun-x86_64.AppImage"
    log_info "AppImage created: $ARTIFACTS_DIR/MidnightSun-x86_64.AppImage"
else
    log_error "AppImage creation failed"
    exit 1
fi

# Optional: Verify with appimagelint
if command -v appimagelint &> /dev/null; then
    log_info "Verifying AppImage..."
    appimagelint "$ARTIFACTS_DIR/MidnightSun-x86_64.AppImage"
fi

log_info "Build complete!"
log_info "AppImage location: $ARTIFACTS_DIR/MidnightSun-x86_64.AppImage"
log_info "Size: $(du -h "$ARTIFACTS_DIR/MidnightSun-x86_64.AppImage" | cut -f1)"
