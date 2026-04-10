# MidnightSun

#### Self-hosted game stream host for Moonlight — Linux-only, headless-first.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)

---

## What is MidnightSun?

MidnightSun is a **Linux-only fork** of [Sunshine](https://github.com/LizardByte/Sunshine), designed specifically for **headless game streaming**. It enables Moonlight clients to stream games from Linux machines without requiring a physical monitor connected.

**Key focus**: Automatic resolution matching — Moonlight requests 4K, MidnightSun streams at 4K. No manual configuration needed.

## Features

### Core Features
- **Headless Operation** — Stream games without a physical monitor attached
- **Auto-Resolution** — Stream resolution automatically matches what Moonlight requests
- **AMD GPU Optimized** — First-class support for AMD GPUs via VAAPI (Bazzite, Steam Deck)
- **KMS/DRM Capture** — High-performance screen capture using Linux graphics stack
- **Wayland Support** — Native Wayland compositor integration
- **PipeWire Integration** — Audio/video capture through PipeWire
- **Moonlight Compatible** — Works with any Moonlight client

### Reliability Features
- **Certificate Persistence** — Trust certificates survive session restarts, no re-pairing needed
- **VKMS Fallback** — Software capture when VKMS unavailable (graceful degradation)
- **Gamescope Compatible** — Works with Steam Deck and Gamescope environments
- **Robust Error Handling** — Clear error messages with actionable remediation steps

### Quality Features
- **Low Latency** — Inherited from Sunshine's optimized streaming pipeline
- **Multiple Resolutions** — Supports 720p, 1080p, 1440p, 4K
- **Codec Support** — H.264, HEVC, AV1 (hardware and software)
- **HDR Ready** — Foundation for future HDR support

## Target Platform

- **OS**: Linux (Bazzite OS, SteamOS 4.x, Fedora, Ubuntu 22.04+)
- **GPU**: AMD (GCN 2.0+), with VAAPI encoding
- **Architecture**: x86_64, aarch64
- **Use Case**: Headless streaming (no physical monitor required)

## Why MidnightSun?

Sunshine is a fantastic project, but it's designed for general-purpose game streaming across multiple platforms. MidnightSun focuses specifically on the Linux headless use case:

| | Sunshine | MidnightSun |
|---|---|---|
| Platforms | Windows, macOS, Linux, FreeBSD | Linux only |
| Display Requirement | Physical monitor | Virtual display (headless) |
| Resolution Config | Manual or client-requested | Automatic (matches Moonlight) |
| Target Hardware | All GPUs | AMD GPU optimized |

When running Sunshine on Linux without a monitor:
- Capture resolution comes from DRM framebuffer (not from Moonlight)
- Headless mode fails or uses wrong resolution
- Manual configuration required

MidnightSun solves this by:
1. Creating virtual displays via VKMS when no physical monitor exists
2. Using Moonlight's requested resolution directly for capture
3. No manual resolution configuration needed

## Installation

### AppImage (Recommended)
```bash
# Download latest release
wget https://github.com/KanonZVE/MidnightSun/releases/latest/download/MidnightSun-x86_64.AppImage

# Make executable
chmod +x MidnightSun-x86_64.AppImage

# Run
./MidnightSun-x86_64.AppImage
```

### Building from Source

#### Requirements
- CMake 3.20+
- GCC 12+ or Clang 16+
- libpipewire, libwayland, libdrm, libegl, libcap
- GBM/AMDKFD headers
- Linux kernel 5.x+ (for VKMS virtual display support)

#### Build Steps
```bash
git clone https://github.com/KanonZVE/MidnightSun.git
cd MidnightSun
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Dependencies (Fedora/Bazzite)
```bash
sudo dnf install cmake gcc-c++ libpipewire-devel libwayland-devel \
    libdrm-devel mesa-libGL-devel libcap-devel
```

## Headless Operation

### How It Works
1. **Detection**: MidnightSun detects when no physical monitor is connected
2. **Virtual Display**: Creates VKMS virtual display at Moonlight's requested resolution
3. **Capture**: Captures from virtual display (or falls back to software mode)
4. **Stream**: Encodes and streams to Moonlight at the exact resolution requested

### Requirements
- Linux kernel 5.x+ with VKMS module available
- CAP_SYS_ADMIN capability (or root) for VKMS module loading
- AMD GPU recommended for optimal VAAPI encoding

### Modes
| Mode | Description | Performance |
|------|-------------|-------------|
| VKMS | Virtual display via kernel module | Best - hardware framebuffer |
| Software | Blank frames when VKMS unavailable | Good - CPU-only capture |
| Gamescope | Uses Gamescope's existing output | Good - Steam Deck compatible |

### Troubleshooting

#### VKMS Module Not Available
```bash
# Check if VKMS is loaded
ls /sys/module/vkms

# Load VKMS module (requires root)
sudo modprobe vkms

# Check if module exists
modinfo vkms
```

#### Permission Denied
```bash
# Grant CAP_SYS_ADMIN to Sunshine binary
sudo setcap cap_sys_admin+ep /path/to/MidnightSun

# Or run as root (not recommended)
sudo ./MidnightSun
```

#### No Displays Detected
```bash
# List DRM devices
ls -la /dev/dri/

# Check for active connectors
cat /sys/class/drm/card*/status
```

#### Certificate Issues
```bash
# Backup exists at sunshine_state.json.backup
cp sunshine_state.json.backup sunshine_state.json
```

## Credits

### Sunshine (LizardByte)
MidnightSun is built on the foundation of **[Sunshine](https://github.com/LizardByte/Sunshine)**, a self-hosted game stream host for Moonlight. Sunshine is developed by the LizardByte team and provides the robust streaming infrastructure that MidnightSun extends.

**Original Sunshine Features:**
- Low-latency game streaming server
- AMD, Intel, and Nvidia GPU encoding support
- Multi-platform client support (Moonlight)
- Web UI for configuration and pairing
- KMS, Wayland, PipeWire capture on Linux

**Sunshine Contributors:**
- [The Sunshine Team (LizardByte)](https://github.com/LizardByte)
- [All Sunshine Contributors](https://github.com/LizardByte/Sunshine/graphs/contributors)

MidnightSun would not be possible without their incredible work on Sunshine.

### Apollo / Vibepollo
- [Apollo](https://github.com/ClassicOldSong/Apollo) — Enhanced streaming features
- [Vibepollo](https://github.com/nicknaccess/Vibepollo) — Quality-of-life improvements

## Support

### GitHub Issues
Report bugs and request features at: [GitHub Issues](https://github.com/KanonZVE/MidnightSun/issues)

### Documentation
- [Architecture Documentation](docs/ARCHITECTURE.md)
- [Implementation Roadmap](docs/ROADMAP.md)
- [Product Requirements](docs/PRD.md)

### Community
- [Moonlight Community](https://moonlight-stream.org)
- [Sunshine Discord](https://discord.gg/sunshine)

## License

Same as Sunshine. See [LICENSE](LICENSE).

## Roadmap

### Completed
- [x] Project setup and branding
- [x] Headless detection (no physical monitor)
- [x] VKMS virtual display integration
- [x] Resolution propagation from Moonlight client
- [x] Software fallback when VKMS unavailable
- [x] Certificate persistence (fix for trust loss)
- [x] Gamescope compatibility (Steam Deck)
- [x] Robust error handling and logging
- [x] Comprehensive documentation

### In Progress
- [ ] Manual testing on Bazzite with AMD GPU
- [ ] Unit tests for non-hardware code
- [ ] AppImage packaging

### Future
- [ ] HDR support
- [ ] AV1 hardware encoding enhancement
- [ ] Multiple simultaneous clients
- [ ] Modern web UI
- [ ] Performance optimization
- [ ] Bazzite OS testing and optimization
- [ ] Documentation and deployment guides
