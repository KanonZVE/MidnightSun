# MidnightSun Build Scripts

## Scripts

### `build-appimage.sh`
Builds the MidnightSun AppImage for portable distribution.

**Usage:**
```bash
cd /path/to/MidnightSun
./scripts/build-appimage.sh
```

**Requirements:**
- Linux (Ubuntu 22.04+ recommended)
- CMake 3.20+
- Ninja build system
- Internet connection (downloads linuxdeploy)

**Output:**
- `artifacts/MidnightSun-x86_64.AppImage`

**Notes:**
- Must be run on Linux (not Windows/macOS)
- Installs build dependencies automatically on Ubuntu/Debian
- Creates AppImage with all dependencies bundled
- Includes VKMS support for headless operation

## Manual Build

If you prefer to build manually:

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt-get install -y \
    build-essential cmake ninja-build \
    libdrm-dev libgl-dev libwayland-dev \
    libx11-xcb-dev libxcb-dri3-dev libxfixes-dev \
    libgtk-3-dev librsvg2-dev libfuse2

# 2. Configure with AppImage flag
mkdir build && cd build
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSUNSHINE_ASSETS_DIR=share/sunshine \
    -DSUNSHINE_EXECUTABLE_PATH=/usr/bin/sunshine \
    -DSUNSHINE_BUILD_APPIMAGE=ON

# 3. Build
ninja

# 4. Install to AppDir
DESTDIR=AppDir ninja install

# 5. Download linuxdeploy
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# 6. Create AppImage
./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    --plugin gtk \
    --executable AppDir/usr/bin/sunshine \
    --desktop-file ../packaging/AppImage/com.midnightsun.app.desktop \
    --output appimage
```

## Testing

After building, test the AppImage:

```bash
# Make executable
chmod +x artifacts/MidnightSun-x86_64.AppImage

# Run headless test
./artifacts/MidnightSun-x86_64.AppImage --help

# Install udev rules (optional)
./artifacts/MidnightSun-x86_64.AppImage --install
```
