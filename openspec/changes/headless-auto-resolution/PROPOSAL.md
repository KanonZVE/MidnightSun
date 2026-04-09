# CHANGE PROPOSAL: Headless Auto-Resolution for MidnightSun

## Change ID: headless-auto-resolution
## Status: proposal
## Date: 2026-04-09

---

## 1. INTENT

Make MidnightSun stream at the resolution requested by the Moonlight client when running headless (no physical monitor connected) on Linux, with AMD GPU.

## 2. SCOPE

### In Scope
- Linux KMS/DRM capture backend
- Virtual display creation when no physical monitor is detected
- Resolution propagation from client request to capture pipeline
- Bazzite OS with AMD GPU (VAAPI encoding)
- Wayland compositor integration

### Out of Scope
- Windows/macOS platforms
- NVIDIA GPU (NVENC) optimization
- Headless without GPU (software-only encoding)
- Dynamic resolution changes mid-stream (stretch goal)

## 3. PROBLEM STATEMENT

When MidnightSun runs on Linux without a physical monitor connected:

1. **Capture resolution comes from hardware** — KMS grabs the DRM framebuffer resolution, which defaults to whatever the compositor decides (often 1024x768 or similar)
2. **Client resolution goes unused in capture** — Moonlight requests a resolution (e.g., 4K), which is used by the encoder but NOT by the capture pipeline
3. **Result**: low-resolution capture, upscaled output — poor quality

The capture resolution and encoding resolution are currently **independent**. When headless, there's no DRM framebuffer to capture from, so capture fails entirely or uses a wrong resolution.

## 4. PROPOSED APPROACH

### Option A: VKMS (Virtual Kernel Mode Setting) — RECOMMENDED
- Load `vkms` kernel module to create a virtual display
- Use DRM API to create a virtual connector/crtc/plane at the requested resolution
- Capture from the virtual display instead of physical hardware
- **Pros**: Clean, kernel-supported, no hardware dependency
- **Cons**: Requires vkms support in kernel (available since Linux 5.x)

### Option B: Memory-based Capture Fallback
- When no DRM framebuffer is found, create a software buffer at the requested resolution
- Feed black/content buffer to encoder
- **Pros**: Simple, works everywhere
- **Cons**: Requires game/app to render to this buffer (no real display)

### Option C: PipeWire Portal Capture
- Use XDG Desktop Portal to request a "virtual display" share
- PipeWire creates a virtual node at requested resolution
- **Pros**: Modern, desktop-portal friendly
- **Cons**: Depends on portal implementation, may not create virtual display

**Recommendation**: Start with Option A (VKMS), with Option B as fallback.

## 5. AFFECTED FILES

| File | Change Type | Description |
|------|------------|-------------|
| `src/platform/linux/kmsgrab.cpp` | Modify | Add headless detection + virtual display creation |
| `src/platform/linux/kmsgrab.h` | Modify | Add virtual display structs |
| `src/video.cpp` | Modify | Propagate client resolution to capture init |
| `src/nvhttp.cpp` | Reference only | Already extracts client resolution |
| `src/config.cpp` | Modify | Add headless mode config option |
| `src/config.h` | Modify | Add headless config struct |
| `src/platform/linux/display_device.cpp` | Modify | Route headless config to capture |

## 6. RISKS & MITIGATIONS

| Risk | Impact | Mitigation |
|------|--------|------------|
| VKMS not available on older kernels | High | Detect vkms availability, fallback to memory-based capture |
| Vulkan/DRM driver incompatibility | Medium | Test with AMD GPU (amdgpu driver) primarily |
| Performance impact of virtual display | Medium | Benchmark capture latency vs physical display |
| Gamescope interference | Low | Ensure vkms device is separate from gamescope's virtual output |

## 7. ROLLBACK PLAN

- Keep all new code behind a feature flag (`--headless` or config option)
- If headless mode fails, MidnightSun falls back to existing behavior (error on no display)
- No changes to existing capture paths unless headless is explicitly enabled

## 8. SUCCESS CRITERIA

- MidnightSun starts and streams when no physical monitor is connected
- Stream resolution matches what Moonlight requests
- Works on Bazzite OS with AMD GPU
- No degradation of normal (with monitor) operation

## 9. TIMELINE ESTIMATE

- Phase 1 (VKMS integration): 1-2 weeks
- Phase 2 (Resolution propagation): 1 week  
- Phase 3 (Testing on Bazzite): 1-2 weeks
- Total: 3-5 weeks
