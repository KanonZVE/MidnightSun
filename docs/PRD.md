# PRODUCT REQUIREMENTS DOCUMENT (PRD)
## MidnightSun - Linux-Only Game Streaming Server

**Version**: 1.0
**Date**: 2026-04-09
**Status**: Draft
**Author**: MidnightSun Development Team

---

## 1. EXECUTIVE SUMMARY

### 1.1 Problem Statement

Sunshine, the leading open-source game streaming server for Moonlight, has critical bugs and quality issues on Linux:

1. **Certificate Trust Loss**: Sunshine loses trust certificates when sessions are closed and reopened, requiring manual re-pairing
2. **No Headless Operation**: Cannot stream when the physical monitor is turned off or disconnected
3. **General Quality Issues**: Various bugs affecting user experience on Linux

These issues make Sunshine unreliable for Linux users who want to use their main computer as a game streaming server.

### 1.2 Product Vision

**MidnightSun** is a Linux-only fork of Sunshine that solves critical reliability issues while adding modern features like HDR, AV1, and multiple simultaneous clients. It provides a seamless, professional game streaming experience for Linux users.

### 1.3 Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Headless Operation | ✅ Works with monitor off | Can stream 1080p60 without physical display |
| Certificate Persistence | ✅ No re-pairing needed | Certificates survive session restart |
| Stream Quality | ✅ Matches client request | Resolution/fps matches Moonlight settings |
| Multiple Clients | ✅ 2+ simultaneous | Can host local multiplayer sessions |
| Distribution | ✅ AppImage available | Single-file portable distribution |

---

## 2. TARGET USERS

### 2.1 Primary Users

**Linux Main Computer Users**
- Run Linux as their primary desktop OS
- Have AMD GPUs (target: Bazzite, SteamOS, Fedora, Ubuntu)
- Use Moonlight for game streaming
- Experience Sunshine's Linux bugs
- Want reliable, professional streaming

### 2.2 User Personas

**Persona 1: Alex - Linux Gamer**
- Uses Bazzite OS with AMD RX 7900 XTX
- Wants to stream games to Steam Deck while PC monitor is off
- Frustrated by Sunshine's certificate bugs
- Needs reliable headless operation

**Persona 2: Maria - Local Multiplayer Enthusiast**
- Runs Ubuntu with AMD GPU
- Wants to host local multiplayer sessions
- Friends connect via Moonlight from tablets/phones
- Needs multiple simultaneous clients

**Persona 3: Carlos - HDR Content Creator**
- Uses Fedora with HDR monitor
- Wants to stream HDR games to TV
- Sunshine doesn't support HDR on Linux
- Needs proper HDR passthrough

---

## 3. FEATURES & REQUIREMENTS

### 3.1 Core Features (Must Have)

#### 3.1.1 Headless Operation
**Priority**: P0 - Critical
**Description**: Stream games without a physical monitor connected

**Requirements**:
- FR-1.1: Detect when no physical monitor is connected
- FR-1.2: Create virtual display via VKMS at client-requested resolution
- FR-1.3: Capture from virtual display without errors
- FR-1.4: Clean up virtual display when session ends
- FR-1.5: Support 720p, 1080p, 1440p, 4K resolutions

**Current Status**: ✅ Phase 1-2 implemented

#### 3.1.2 Certificate Persistence
**Priority**: P0 - Critical
**Description**: Trust certificates survive session restarts

**Requirements**:
- FR-2.1: Certificates stored in persistent location
- FR-2.2: Certificates loaded on startup
- FR-2.3: No re-pairing needed after session close/open
- FR-2.4: Support multiple paired clients

**Current Status**: ⏳ Not implemented

#### 3.1.3 Reliable Streaming
**Priority**: P0 - Critical
**Description**: Stable streaming without connection drops

**Requirements**:
- FR-3.1: No unexpected disconnections
- FR-3.2: Automatic reconnection support
- FR-3.3: Low latency (<30ms on LAN)
- FR-3.4: Stable frame delivery

**Current Status**: ✅ Inherited from Sunshine

### 3.2 Enhanced Features (Should Have)

#### 3.2.1 HDR Support
**Priority**: P1 - High
**Description**: HDR10 passthrough for compatible displays

**Requirements**:
- FR-4.1: Detect HDR capability of display
- FR-4.2: Negotiate HDR with Moonlight client
- FR-4.3: HDR metadata passthrough
- FR-4.4: HDR tone mapping for SDR displays

**Current Status**: ⏳ Partial (Sunshine has Windows HDR)

#### 3.2.2 AV1 Codec Enhancement
**Priority**: P1 - High
**Description**: Better AV1 encoding support

**Requirements**:
- FR-5.1: Hardware AV1 encoding (AMD AMF)
- FR-5.2: Software AV1 encoding (SVT-AV1)
- FR-5.3: AV1 10-bit HDR support
- FR-5.4: Auto-detect AV1 capability

**Current Status**: ⏳ Partial (Sunshine has basic AV1)

#### 3.2.3 Multiple Simultaneous Clients
**Priority**: P1 - High
**Description**: Host local multiplayer sessions

**Requirements**:
- FR-6.1: Support 2+ Moonlight clients simultaneously
- FR-6.2: Each client gets own controller input
- FR-6.3: Independent resolution per client
- FR-6.4: Synchronized game state

**Current Status**: ⏳ Not implemented

#### 3.2.4 Modern Web UI
**Priority**: P2 - Medium
**Description**: Professional, responsive web interface

**Requirements**:
- FR-7.1: Modern React/Vue frontend
- FR-7.2: Mobile-responsive design
- FR-7.3: Real-time status updates
- FR-7.4: Easy configuration management

**Current Status**: ⏳ Not implemented

### 3.3 Nice to Have (Could Have)

#### 3.3.1 Game Integration
- FR-8.1: Auto-detect installed games
- FR-8.2: Game artwork/icons
- FR-8.3: Launch game directly from Moonlight

#### 3.3.2 Performance Monitoring
- FR-9.1: Real-time latency display
- FR-9.2: Bandwidth monitoring
- FR-9.3: GPU utilization tracking

#### 3.3.3 Cloud Save Sync
- FR-10.1: Sync saves across devices
- FR-10.2: Support Steam Cloud integration

---

## 4. TECHNICAL ARCHITECTURE

### 4.1 System Overview

```
Moonlight Client(s)
        │
        ▼
┌─────────────────────────────────────┐
│         MidnightSun Server          │
├─────────────────────────────────────┤
│  HTTP/RTSP API (Sunshine core)      │
├─────────────────────────────────────┤
│  Session Management                 │
├─────────────────────────────────────┤
│  Video Pipeline                     │
│  ┌─────────────┐  ┌─────────────┐  │
│  │   Capture   │→│   Encoder   │  │
│  │  (KMS/VKMS) │  │ (VAAPI/AV1)│  │
│  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────┤
│  Platform Layer (Linux)             │
│  - Certificate Management           │
│  - Display Management               │
│  - Input Handling                   │
└─────────────────────────────────────┘
```

### 4.2 Key Components

| Component | Description | Status |
|-----------|-------------|--------|
| HTTP/RTSP API | Sunshine's core networking | ✅ Inherited |
| Session Management | Client connections, pairing | ✅ Inherited |
| Video Pipeline | Capture, encode, stream | ✅ Inherited + Headless |
| Certificate Manager | Persistent trust storage | ⏳ To implement |
| Display Manager | Headless/virtual displays | ✅ Phase 1-2 |
| HDR Manager | HDR detection/passthrough | ⏳ To implement |
| Multi-Client Manager | Multiple simultaneous streams | ⏳ To implement |

---

## 5. DISTRIBUTION

### 5.1 AppImage

**Format**: Single-file portable application
**Requirements**:
- Self-contained with all dependencies
- Works on any Linux distribution
- No installation required
- Auto-updates via built-in mechanism

**Build Process**:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_APPIMAGE=ON
make -j$(nproc)
make appimage
```

### 5.2 Alternative Distributions

| Format | Priority | Notes |
|--------|----------|-------|
| AppImage | P0 | Primary distribution |
| Flatpak | P2 | Optional, Flathub |
| Native (.rpm, .deb) | P3 | Community maintained |

---

## 6. TESTING STRATEGY

### 6.1 Test Matrix

| Test Category | Scenarios | Platform |
|--------------|-----------|----------|
| Headless Operation | Monitor off, resolution changes | Bazzite, Fedora |
| Certificate Persistence | Session restart, multiple clients | All supported |
| HDR Streaming | HDR10, SDR fallback | HDR-capable systems |
| Multi-Client | 2-4 simultaneous clients | All supported |
| Performance | Latency, CPU, GPU | Various hardware |

### 6.2 Test Environments

1. **Bazzite OS** - Primary target (Steam Deck-like)
2. **Fedora 39+** - AMD GPU testing
3. **Ubuntu 22.04+** - LTS compatibility
4. **Steam Deck** - Gamescope compatibility

---

## 7. SUPPORT & DOCUMENTATION

### 7.1 Documentation

| Document | Description | Priority |
|----------|-------------|----------|
| README.md | Project overview | P0 |
| INSTALL.md | Installation guide | P0 |
| CONFIG.md | Configuration options | P0 |
| HEADLESS.md | Headless operation guide | P0 |
| HDR.md | HDR setup guide | P1 |
| TROUBLESHOOTING.md | Common issues | P0 |
| API.md | Developer API reference | P2 |

### 7.2 Support Channels

1. **GitHub Issues** - Primary support channel
2. **Documentation** - Self-service troubleshooting
3. **Community** - Discord/Matrix (optional)

---

## 8. ROADMAP

### 8.1 Phase 1: Foundation (Completed)
- [x] HeadlessDisplay class (VKMS)
- [x] Headless detection
- [x] KMS integration

### 8.2 Phase 2: Core (Current)
- [ ] Certificate persistence
- [ ] Headless testing on Bazzite
- [ ] AppImage packaging

### 8.3 Phase 3: Enhancement
- [ ] HDR support
- [ ] AV1 enhancement
- [ ] Multi-client support

### 8.4 Phase 4: Polish
- [ ] Modern Web UI
- [ ] Performance optimization
- [ ] Documentation completion

---

## 9. RISKS & MITIGATIONS

| Risk | Impact | Mitigation |
|------|--------|------------|
| VKMS not available | High | Software fallback |
| HDR complexity | Medium | Incremental implementation |
| Multi-client complexity | Medium | Start with 2 clients |
| Gamescope conflicts | Medium | Detection + fallback |

---

## 10. APPENDIX

### 10.1 Glossary

| Term | Definition |
|------|------------|
| VKMS | Virtual Kernel Mode Setting |
| KMS | Kernel Mode Setting |
| VAAPI | Video Acceleration API |
| HDR | High Dynamic Range |
| AV1 | AOMedia Video 1 codec |
| AppImage | Portable Linux application format |

### 10.2 References

- Sunshine: https://github.com/LizardByte/Sunshine
- Moonlight: https://moonlight-stream.org
- VKMS: https://docs.kernel.org/gpu/vkms.html
