# TECHNICAL DESIGN: Headless Auto-Resolution for MidnightSun

## Change ID: headless-auto-resolution
## Version: 1.0
## Date: 2026-04-09

---

## 1. ARCHITECTURE OVERVIEW

### Current Architecture (Problem)
```
Moonlight Request
    ↓
nvhttp.cpp (make_launch_session)
    ↓
launch_session.width/height = 1920x1080
    ↓
video::config_t { width=1920, height=1080 }
    ↓
Encoder (uses 1920x1080 for output)
    ↓
Capture (uses DRM framebuffer = 1024x768 or FAILS in headless)
    ↓
Incompatible: capture resolution ≠ encoding resolution
```

### Proposed Architecture (Solution)
```
Moonlight Request
    ↓
nvhttp.cpp (make_launch_session)
    ↓
launch_session.width/height = 1920x1080
    ↓
video::config_t { width=1920, height=1080 }
    ↓
Headless Display Manager (NEW)
    ├─ Detect: Is physical display connected? → No
    ├─ Create: Virtual display via VKMS at 1920x1080
    └─ Route: Capture pipeline → Virtual display
    ↓
Capture (DRM captures from virtual display = 1920x1080)
    ↓
Encoder (uses 1920x1080 for input + output)
    ↓
Stream
```

---

## 2. COMPONENT DESIGN

### 2.1 Headless Display Manager (NEW)

**File**: `src/platform/linux/headless_display.cpp` (NEW)
**File**: `src/platform/linux/headless_display.h` (NEW)

```
class HeadlessDisplay {
public:
    // Initialize VKMS virtual display
    static std::unique_ptr<HeadlessDisplay> create(
        int width, 
        int height,
        int refresh_rate
    );
    
    // Get DRM file descriptor for capture
    int get_drm_fd() const;
    
    // Get connector ID for KMS grab
    uint32_t get_connector_id() const;
    
    // Cleanup
    ~HeadlessDisplay();

private:
    int drm_fd_;
    uint32_t crtc_id_;
    uint32_t connector_id_;
    uint32_t encoder_id_;
    uint32_t framebuffer_id_;
    
    bool setup_vkms(int w, int h, int refresh);
    void cleanup();
};
```

### 2.2 KMS Capture Integration

**File**: `src/platform/linux/kmsgrab.cpp` (MODIFY)

Changes in `display_t::init()`:
```cpp
// BEFORE: Direct DRM scan, fail if no display
if (!plane->fb_id) {
    continue;  // Skip unused planes
}

// AFTER: Detect headless, create virtual display
if (all_planes_empty) {
    auto headless = HeadlessDisplay::create(
        config.width, config.height, config.framerate
    );
    if (headless) {
        // Re-scan with virtual display active
        setup_capture_from_headless(headless);
    } else {
        // Fallback to software capture
        setup_software_capture(config);
    }
}
```

### 2.3 Resolution Propagation

**File**: `src/video.cpp` (MODIFY)

The capture initialization needs access to the client's requested resolution:
```cpp
// In capture_thread or init_capture
auto client_config = config;  // Already has width/height from Moonlight

// Pass to display init
auto display = platf::display_t::make_display(
    client_config.width,    // 1920
    client_config.height,   // 1080
    client_config.framerate // 60
);
```

### 2.4 Configuration

**File**: `src/config.cpp` (MODIFY)

Add headless configuration:
```cpp
// New config option
struct headless_t {
    bool enabled = false;  // Auto-detect by default
    int fallback_width = 1920;
    int fallback_height = 1080;
    int fallback_framerate = 60;
    bool use_vkms = true;  // Prefer VKMS over software
};
```

---

## 3. FLOW DIAGRAM

### Sequence: Headless Stream Start

```
1. Moonlight → MidnightSun
   GET /launch?mode=1920x1080x60
   
2. nvhttp.cpp::make_launch_session()
   → Parse: width=1920, height=1080, fps=60
   → launch_session.width = 1920
   → launch_session.height = 1080
   
3. display_device::configure_display()
   → Check physical display → NONE
   → Activate headless mode
   
4. HeadlessDisplay::create(1920, 1080, 60)
   → modprobe vkms
   → Create DRM virtual connector
   → Create virtual CRTC
   → Create framebuffer at 1920x1080
   → Return virtual display fd
   
5. kmsgrab::display_t::init()
   → Open DRM from HeadlessDisplay
   → Scan planes → finds virtual framebuffer
   → Capture from virtual display
   
6. video.cpp::capture_thread()
   → Capture: 1920x1080 from virtual display
   → Encode: 1920x1080 via VAAPI (AMD)
   → Stream to Moonlight
   
7. Session ends
   → HeadlessDisplay::~HeadlessDisplay()
   → Destroy virtual display
   → Unload vkms module
```

---

## 4. VKMS (Virtual Kernel Mode Setting)

### What is VKMS?
- Kernel module that creates a virtual display connector
- Works with any GPU (including AMD)
- Creates `/dev/dri/cardX` entries without physical hardware

### Usage Pattern
```c
// 1. Open DRM device
int fd = open("/dev/dri/renderD128", O_RDWR);

// 2. Create dumb buffer for framebuffer
struct drm_mode_create_dumb create = {
    .width = 1920,
    .height = 1080,
    .bpp = 32
};
ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

// 3. Create framebuffer from dumb buffer
uint32_t fb_id;
drmModeAddFB(fd, 1920, 1080, 24, 32, create.pitch, create.handle, &fb_id);

// 4. Find/connect to virtual connector
// (Scan existing connectors, vkms creates one automatically)
```

### Bazzite Compatibility
- VKMS is included in Linux kernel 5.x+
- Bazzite uses modern kernels (6.x+) → VKMS available
- `modprobe vkms` loads the module

---

## 5. FALLBACK STRATEGY

```
Headless Detected
    │
    ├─ Try VKMS
    │   ├─ modprobe vkms → Success
    │   ├─ Create virtual display → Success
    │   └─ Return HeadlessDisplay
    │
    ├─ Try Software Buffer
    │   ├─ Allocate software framebuffer at requested resolution
    │   ├─ Pass to capture pipeline as dummy source
    │   └─ Return SoftwareDisplay
    │
    └─ Error
        └─ Log: "Cannot create display for headless operation"
        └─ Return nullptr
```

---

## 6. TESTING STRATEGY

### Manual Testing (Bazzite)
1. Disconnect physical monitor
2. Start MidnightSun
3. Connect from Moonlight at various resolutions
4. Verify stream quality matches requested resolution
5. Reconnect monitor → verify normal operation

### Automated Testing (Limited)
- VKMS can be tested in containers (limited)
- Mock DRM API calls for unit tests
- Integration tests require actual hardware

### Test Matrix
| Test | Monitor | Resolution | Expected |
|------|---------|-----------|----------|
| Headless 1080p | Disconnected | 1920x1080 | Virtual display @ 1920x1080 |
| Headless 4K | Disconnected | 3840x2160 | Virtual display @ 3840x2160 |
| With Monitor | Connected | 1920x1080 | Physical display used |
| VKMS Unavailable | Disconnected | Any | Software fallback |

---

## 7. ARCHITECTURE DECISION RECORDS

### ADR-001: Use VKMS as Primary Virtual Display
- **Status**: Proposed
- **Context**: Need virtual display for headless operation
- **Decision**: Use VKMS kernel module
- **Consequences**: Requires Linux 5.x+, works with AMD GPU
- **Alternatives**: EGL headless (complex), dummy X server (outdated)

### ADR-002: Integrate with Existing KMS Capture
- **Status**: Proposed  
- **Context**: KMS capture already exists for physical displays
- **Decision**: Extend KMS capture to support virtual displays
- **Consequences**: Minimal code duplication, reuses existing DRM path
- **Alternatives**: New separate capture backend (unnecessary complexity)

### ADR-003: Propagate Resolution from Client to Capture
- **Status**: Proposed
- **Context**: Currently capture resolution is independent of client request
- **Decision**: Pass client resolution to capture initialization
- **Consequences**: Capture adapts to what Moonlight requests
- **Alternatives**: Configuration-only resolution (inflexible)

---

## 8. FILE CHANGES SUMMARY

| File | Type | Description |
|------|------|-------------|
| `src/platform/linux/headless_display.h` | NEW | Virtual display class declaration |
| `src/platform/linux/headless_display.cpp` | NEW | VKMS implementation |
| `src/platform/linux/kmsgrab.cpp` | MODIFY | Headless detection + virtual display integration |
| `src/platform/linux/kmsgrab.h` | MODIFY | Add headless display support |
| `src/video.cpp` | MODIFY | Pass client resolution to capture init |
| `src/config.cpp` | MODIFY | Add headless config options |
| `src/config.h` | MODIFY | Add headless config struct |
| `src/display_device.cpp` | MODIFY | Route headless to capture |
| `src/platform/linux/display_device.cpp` | MODIFY | Linux-specific headless routing |
