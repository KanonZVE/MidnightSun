# MidnightSun

#### Self-hosted game stream host for Moonlight — Linux-only, headless-first.

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)

---

## What is MidnightSun?

MidnightSun is a **Linux-only fork** of [Sunshine](https://github.com/LizardByte/Sunshine), designed specifically for **headless game streaming**. It enables Moonlight clients to stream games from Linux machines without requiring a physical monitor connected.

**Key focus**: Automatic resolution matching — Moonlight requests 4K, MidnightSun streams at 4K. No manual configuration needed.

## Features

- **Headless Operation** — Stream games without a physical monitor attached
- **Auto-Resolution** — Stream resolution automatically matches what Moonlight requests
- **AMD GPU Optimized** — First-class support for AMD GPUs via VAAPI (Bazzite, Steam Deck)
- **KMS/DRM Capture** — High-performance screen capture using Linux graphics stack
- **Wayland Support** — Native Wayland compositor integration
- **PipeWire Integration** — Audio/video capture through PipeWire
- **Moonlight Compatible** — Works with any Moonlight client

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

## Building from Source

### Requirements
- CMake 3.20+
- GCC 12+ or Clang 16+
- libpipewire, libwayland, libdrm, libegl
- GBM/AMDKFD headers
- Linux kernel 5.x+ (for VKMS virtual display support)

### Build Steps
```bash
git clone https://github.com/KanonZVE/MidnightSun.git
cd MidnightSun
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Architecture

MidnightSun inherits Sunshine's well-designed architecture:

```
Client (Moonlight)
    ↓
RTSP/HTTP API
    ↓
Session Manager
    ↓
┌─────────────┐
│  Capture    │ ← KMS/DRM (virtual or physical display)
│  (VKMS)    │
└─────────────┘
    ↓
┌─────────────┐
│  Encoder    │ ← VAAPI (AMD), NVENC, Software
└─────────────┘
    ↓
┌─────────────┐
│  Stream     │ ← RTP/UDP to Moonlight
└─────────────┘
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

## License

Same as Sunshine. See [LICENSE](LICENSE).

## Roadmap

- [x] Project setup and branding
- [ ] Headless detection (no physical monitor)
- [ ] VKMS virtual display integration
- [ ] Resolution propagation from Moonlight client
- [ ] Software fallback when VKMS unavailable
- [ ] Bazzite OS testing and optimization
- [ ] Documentation and deployment guides
