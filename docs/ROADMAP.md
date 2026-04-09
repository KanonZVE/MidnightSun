# MIDNIGHTSUN - HEADLESS AUTO-RESOLUTION ROADMAP

## Document Purpose
Detailed implementation roadmap with contingencies, limitations, and testing strategy for headless auto-resolution feature.

---

## EXECUTIVE SUMMARY

**Objective**: Enable MidnightSun to stream games at the resolution requested by Moonlight clients when running headless (no physical monitor) on Linux with AMD GPU.

**Core Solution**: Create virtual displays via VKMS (Virtual Kernel Mode Setting) that match the client's requested resolution, eliminating the capture-encoder resolution mismatch.

**Timeline**: 6-8 weeks (12-16 tasks across 4 phases)

---

## PHASE 1: INFRASTRUCTURE (Week 1-2)

### Task 1.1: HeadlessDisplay Class Foundation
**File**: `src/platform/linux/headless.h` + `headless.cpp` (NEW)
**Priority**: HIGH

```
class HeadlessDisplay {
public:
    static std::unique_ptr<HeadlessDisplay> create(
        int width, int height, int refresh_rate);
    
    int get_drm_fd() const;
    uint32_t get_connector_id() const;
    uint32_t get_crtc_id() const;
    uint32_t get_framebuffer_id() const;
    
    bool is_valid() const;
    
    ~HeadlessDisplay();

private:
    int drm_fd_ = -1;
    uint32_t connector_id_ = 0;
    uint32_t crtc_id_ = 0;
    uint32_t encoder_id_ = 0;
    uint32_t framebuffer_id_ = 0;
    uint32_t plane_id_ = 0;
    
    bool setup_vkms(int w, int h, int refresh);
    void cleanup();
};
```

**Implementation Steps**:
1. Check if vkms module is loaded (`/sys/module/vkms`)
2. Load vkms module if needed (`modprobe vkms`)
3. Open DRM device (`/dev/dri/cardX`)
4. Create dumb buffer for framebuffer
5. Create framebuffer from dumb buffer
6. Find/create virtual connector
7. Find/create CRTC
8. Configure mode (resolution + refresh rate)

**Acceptance Criteria**:
- [ ] Can detect VKMS module availability
- [ ] Can load VKMS module (requires root or CAP_SYS_ADMIN)
- [ ] Can create virtual framebuffer at arbitrary resolution
- [ ] Can provide valid DRM fd and IDs to capture pipeline
- [ ] Cleanup destroys all allocated resources

**Limitations & Contingencies**:
| Limitation | Severity | Contingency |
|-----------|----------|-------------|
| VKMS not compiled in kernel | HIGH | Software fallback |
| Module loading requires root | HIGH | Provide udev rules + systemd service |
| VKMS doesn't expose render node | MEDIUM | Use RAM-based capture path |
| Secure boot blocks module loading | MEDIUM | Document requirement, software fallback |

---

### Task 1.2: VKMS Detection & Device Identification
**File**: `src/platform/linux/headless.cpp`
**Priority**: HIGH

**Implementation Steps**:
1. Enumerate all `/dev/dri/card*` devices
2. For each device:
   - Open with `open()`
   - Call `drmGetVersion()`
   - Check if `drm_driver->name == "vkms"`
3. Build list of VKMS devices
4. Return VKMS device path or error

**Key Function**:
```cpp
bool is_vkms_card(int fd) {
    drmVersion *ver = drmGetVersion(fd);
    bool result = (ver && strcmp(ver->name, "vkms") == 0);
    drmFreeVersion(ver);
    return result;
}
```

**Acceptance Criteria**:
- [ ] Can enumerate all DRM cards
- [ ] Can identify VKMS cards by driver name
- [ ] Can identify AMD cards by driver name ("amdgpu")
- [ ] Can skip VKMS cards when searching for VAAPI render nodes

---

### Task 1.3: Headless State Detection
**File**: `src/platform/linux/misc.cpp`
**Priority**: HIGH

**Detection Logic**:
```
is_headless():
  │
  ├─ Check: kms_display_names() returns empty?
  │   → If YES: likely headless
  │
  ├─ Check: All detected cards are VKMS?
  │   → If YES: headless (only virtual displays)
  │
  ├─ Check: No active CRTCs with attached planes?
  │   → If YES: headless (displays disconnected)
  │
  └─ Return: true/false
```

**Acceptance Criteria**:
- [ ] Returns true when no physical monitor connected
- [ ] Returns false when physical monitor exists
- [ ] Handles multi-monitor scenarios correctly
- [ ] Works with Gamescope environment

---

## PHASE 2: CORE IMPLEMENTATION (Week 3-4)

### Task 2.1: KMS Capture Integration
**File**: `src/platform/linux/kmsgrab.cpp`
**Priority**: HIGH

**Integration Point**: `kms::display_t::init()` (line 611)

**Changes**:
```cpp
int init(const std::string &display_name, const ::video::config_t &config) {
    // Existing code...
    
    // NEW: Check if headless
    if (monitors.empty() || is_headless_mode_requested()) {
        // Try VKMS virtual display
        headless_ = HeadlessDisplay::create(
            config.width, config.height, config.framerate);
        
        if (headless_ && headless_->is_valid()) {
            // Use VKMS device for capture
            card.init(headless_->get_drm_fd());
            // Configure capture from VKMS framebuffer
            setup_headless_capture(headless_.get());
            return 0;
        } else {
            // VKMS failed, try software fallback
            BOOST_LOG(warning) << "VKMS unavailable, using software fallback";
            return init_software_fallback(config);
        }
    }
    
    // Existing physical display code...
}
```

**Acceptance Criteria**:
- [ ] When headless detected, creates VKMS virtual display
- [ ] Virtual display uses client's requested resolution
- [ ] Capture works from virtual display
- [ ] Physical display path unchanged when monitor exists

---

### Task 2.2: Resolution Propagation
**File**: `src/video.cpp`
**Priority**: HIGH

**Key Change**: Pass client resolution to display creation

In `captureThread()` (line 1253):
```cpp
// BEFORE:
disp = platf::display(encoder.platform_formats->dev_type,
                      display_names[display_p],
                      capture_ctxs.front().config);

// AFTER:
auto capture_config = capture_ctxs.front().config;
// capture_config already has width/height from Moonlight's RTSP ANNOUNCE

disp = platf::display(encoder.platform_formats->dev_type,
                      display_names[display_p],
                      capture_config);
```

The key insight: `capture_ctxs.front().config` already contains the client's resolution from `config.monitor` (populated in `rtsp.cpp:989`). We just need to ensure this flows through to the display creation.

**Acceptance Criteria**:
- [ ] Display dimensions match client request when headless
- [ ] Display dimensions unchanged when physical monitor exists
- [ ] No scaling needed when display matches encoder dimensions

---

### Task 2.3: Display Source Registration
**File**: `src/platform/linux/misc.cpp`
**Priority**: HIGH

**Changes in `platf::init()`**:
```cpp
// After existing source detection:
sources[source::HEADLESS] = verify_headless();

// Add HEADLESS to source enum
enum source_e : std::size_t {
    NVFBC, WAYLAND, KMS, X11, PORTAL,
    HEADLESS,  // NEW
    MAX_FLAGS
};
```

**Changes in `platf::display()`**:
```cpp
std::shared_ptr<display_t> display(mem_type_e hwdevice_type,
                                    const std::string &display_name,
                                    const video::config_t &config) {
    // NEW: Headless path
    if (sources[source::HEADLESS]) {
        return headless_display(hwdevice_type, display_name, config);
    }
    
    // Existing paths...
}
```

---

### Task 2.4: Session Lifecycle Integration
**File**: `src/video.cpp` + `src/platform/linux/headless.cpp`
**Priority**: MEDIUM

**Implementation**:
1. Virtual display created at session start (in `captureThread`)
2. Virtual display destroyed at session end (in destructor)
3. Multiple sessions handled correctly (one virtual display per session)

**Acceptance Criteria**:
- [ ] Virtual display created when first client connects
- [ ] Virtual display destroyed when last client disconnects
- [ ] No memory leaks
- [ ] Multiple concurrent sessions work correctly

---

## PHASE 3: FALLBACK & ERROR HANDLING (Week 5-6)

### Task 3.1: Software Capture Fallback
**File**: `src/platform/linux/headless.cpp` + `src/platform/linux/kmsgrab.cpp`
**Priority**: MEDIUM

**When VKMS Unavailable**:
```
HeadlessDisplay::create()
    │
    ├─ Try VKMS: modprobe vkms
    │   ├─ Success → Create virtual display
    │   └─ Failure → Continue to software
    │
    ├─ Try Software Buffer:
    │   ├─ Allocate RAM buffer at client resolution
    │   ├─ Create SoftwareDisplay wrapper
    │   └─ Return SoftwareDisplay
    │
    └─ Error: "Cannot create headless display"
```

**Software Fallback Implementation**:
```cpp
class SoftwareDisplay : public platf::display_t {
public:
    int init(const video::config_t &config) {
        this->width = config.width;
        this->height = config.height;
        
        // Allocate framebuffer
        frame_buffer.resize(width * height * 4); // RGBA
        
        return 0;
    }
    
    capture_e capture(...) override {
        // Fill with black or capture from compositor
        memset(frame_buffer.data(), 0, frame_buffer.size());
        
        // Create img_t pointing to buffer
        auto img = std::make_shared<img_t>();
        img->data = frame_buffer.data();
        img->width = width;
        img->height = height;
        img->pixel_pitch = 4;
        img->row_pitch = width * 4;
        
        push_callback(img, true);
        return capture_e::ok;
    }
};
```

---

### Task 3.2: Error Handling & Logging
**Files**: All new files
**Priority**: MEDIUM

**Error Scenarios & Messages**:
| Error | Log Level | Message |
|-------|-----------|---------|
| VKMS module not found | ERROR | "VKMS module not available. Install kernel module or use software fallback." |
| Module loading failed | ERROR | "Failed to load VKMS module. Root or CAP_SYS_ADMIN required." |
| No DRM devices found | ERROR | "No DRM devices found. Is a GPU present?" |
| VKMS card creation failed | ERROR | "Failed to create VKMS virtual display." |
| Software fallback active | WARNING | "VKMS unavailable. Using software capture (reduced performance)." |

---

### Task 3.3: Gamescope Compatibility
**File**: `src/platform/linux/misc.cpp` + `src/platform/linux/headless.cpp`
**Priority**: MEDIUM

**Detection**:
```cpp
bool is_gamescope_active() {
    // Check environment variables
    if (getenv("GAMESCOPE_WIDTH") || getenv("GAMESCOPE_MODELIST"))
        return true;
    
    // Check for Gamescope Wayland protocol
    // (wlroots virtual output protocol)
    
    return false;
}
```

**Handling**:
- If Gamescope active AND no VKMS needed → Use Gamescope's output
- If Gamescope active AND VKMS requested → Create VKMS separately
- Ensure VKMS device is different from Gamescope's device

---

## PHASE 4: TESTING & POLISH (Week 7-8)

### Task 4.1: Manual Testing Matrix
**Platform**: Bazzite OS + AMD GPU

| Test Case | Monitor | Resolution | Expected |
|-----------|---------|-----------|----------|
| Headless 1080p | Disconnected | 1920x1080 | VKMS @ 1920x1080 |
| Headless 1440p | Disconnected | 2560x1440 | VKMS @ 2560x1440 |
| Headless 4K | Disconnected | 3840x2160 | VKMS @ 3840x2160 |
| With Monitor | Connected | 1920x1080 | Physical display used |
| VKMS Unavailable | Disconnected | Any | Software fallback |
| Gamescope Active | Steam Deck | Any | Gamescope output used |
| Permission Denied | Any | Any | Clear error message |

---

### Task 4.2: Performance Benchmarking
**Metrics**:
- Capture latency (ms per frame)
- CPU utilization (%)
- Memory bandwidth (MB/s)
- Frame timing consistency (ms variance)

**Acceptance Criteria**:
| Resolution | Max Latency | Max CPU | Notes |
|------------|-------------|---------|-------|
| 1920x1080 | < 16ms | < 10% | 60fps target |
| 2560x1440 | < 16ms | < 15% | 60fps target |
| 3840x2160 | < 16ms | < 25% | 60fps target |

---

### Task 4.3: Documentation
**Files to Create/Update**:
- `README.md` - Add headless section
- `docs/HEADLESS.md` - Headless operation guide
- `docs/TROUBLESHOOTING.md` - Common issues

**Content**:
- How to enable headless mode
- VKMS requirements
- Software fallback
- Performance implications
- Known limitations

---

## CONTINGENCY MATRIX

### If VKMS Fails Entirely
**Trigger**: VKMS module unavailable or doesn't work
**Action**:
1. Implement pure software framebuffer fallback
2. Document performance limitations
3. Provide pre-built VKMS module packages for Bazzite

### If AMD GPU + VKMS Conflicts
**Trigger**: VAAPI encoding fails with VKMS
**Action**:
1. Ensure VKMS cards are filtered from VAAPI enumeration
2. Use explicit device paths for VAAPI
3. Test on multiple AMD GPU generations

### If Gamescope Interferes
**Trigger**: VKMS conflicts with Gamescope virtual output
**Action**:
1. Detect Gamescope environment
2. Use Gamescope's existing output
3. Only create VKMS when Gamescope not present

### If Flatpak Required
**Trigger**: Users need Flatpak deployment
**Action**:
1. Detect sandboxed environment
2. Provide clear error: "Headless requires native installation"
3. Consider Portal path for non-headless Flatpak

---

## IMPLEMENTATION ORDER

```
Phase 1 (Infrastructure) - Week 1-2
├── 1.1 HeadlessDisplay Class ──────────────────┐
├── 1.2 VKMS Detection ─────────────────────────┼──→ Phase 2
└── 1.3 Headless Detection ─────────────────────┘

Phase 2 (Core) - Week 3-4
├── 2.1 KMS Integration ←── 1.1, 1.3
├── 2.2 Resolution Propagation ←── 1.1
├── 2.3 Source Registration ←── 1.2, 1.3
└── 2.4 Session Lifecycle ←── 2.1, 2.2

Phase 3 (Fallback) - Week 5-6
├── 3.1 Software Fallback ←── 1.1
├── 3.2 Error Handling ←── 2.1, 3.1
└── 3.3 Gamescope ←── 1.2, 2.1

Phase 4 (Testing) - Week 7-8
├── 4.1 Manual Testing ←── 2.1, 2.2, 2.3, 3.1
├── 4.2 Benchmarking ←── 2.1, 3.1
└── 4.3 Documentation ←── 4.1
```

---

## RISK REGISTER

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| VKMS not available on target kernel | LOW | HIGH | Software fallback + documentation |
| AMD GPU + VKMS DMA-BUF conflict | MEDIUM | HIGH | Filter VKMS from VAAPI, use RAM path |
| Gamescope interference | MEDIUM | MEDIUM | Detect and use Gamescope output |
| Flatpak deployment issues | LOW | MEDIUM | Document native requirement |
| Performance degradation | MEDIUM | MEDIUM | Benchmark and optimize software path |
| Permission issues | HIGH | HIGH | Provide udev rules, systemd service |

---

## SUCCESS CRITERIA

- [ ] MidnightSun streams headless without physical monitor
- [ ] Stream resolution matches Moonlight client request
- [ ] Works on Bazzite OS with AMD GPU
- [ ] No regression in normal (with monitor) operation
- [ ] Proper error handling for all failure modes
- [ ] Performance acceptable (1080p60 with < 10% CPU)
