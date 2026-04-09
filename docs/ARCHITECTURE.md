# MIDNIGHTSUN - SUNSHINE ARCHITECTURE DOCUMENTATION

## Document Purpose
Complete technical documentation of Sunshine's architecture for implementing headless auto-resolution in MidnightSun.

---

## TABLE OF CONTENTS

1. [Initialization Flow](#1-initialization-flow)
2. [Connection Handling (HTTP/RTSP)](#2-connection-handling)
3. [Video Capture Pipeline](#3-video-capture-pipeline)
4. [Encoder System](#4-encoder-system)
5. [Resolution Handling](#5-resolution-handling)
6. [Display Device Management](#6-display-device-management)
7. [Headless Implementation Plan](#7-headless-implementation-plan)

---

## 1. INITIALIZATION FLOW

### Main Entry Point (`src/main.cpp`)

```
main(argc, argv)
  │
  ├─ [143] Store argv for restart
  ├─ [158] Set UTF-8 locale
  ├─ [161] Create global mail bus
  ├─ [164] config::parse(argc, argv) ─── PARSE CONFIG
  │   └─ Reads sunshine.conf, applies defaults
  ├─ [168] logging::init()
  ├─ [204] display_device::init()
  ├─ [297] task_pool.start(1)
  ├─ [300-333] Register signal handlers
  ├─ [340] proc::refresh() ─── LOAD APPS
  ├─ [345] platf::init() ─── PLATFORM INIT *** KEY ***
  │   ├─ Set AMD_DEBUG=lowlatencyenc
  │   ├─ Set RADV_PERFTEST=video_encode
  │   ├─ gbm::init()
  │   ├─ Detect window_system (WAYLAND/X11/NONE)
  │   └─ Detect capture sources
  ├─ [362] video::probe_encoders() ─── DETECT ENCODERS
  ├─ [366] http::init()
  ├─ [392] nvhttp::start() ─── GameStream HTTP
  ├─ [393] confighttp::start() ─── Web UI HTTPS
  ├─ [394] rtsp_stream::start() ─── RTSP server
  └─ [418] mainThreadLoop(shutdown_event)
```

### Platform Init (`src/platform/linux/misc.cpp:1084-1150`)

```
platf::init()
  │
  ├─ Window System Detection:
  │   ├─ WAYLAND_DISPLAY env → window_system = WAYLAND
  │   └─ DISPLAY env → window_system = X11
  │
  └─ Capture Source Detection:
      ├─ [NVFBC]  NVIDIA Frame Buffer Capture (CUDA)
      ├─ [WAYLAND] wlroots screencopy protocol
      ├─ [KMS]    DRM/KMS direct access ← HEADLESS TARGET
      ├─ [X11]    X11 XShm/XGetImage
      └─ [PORTAL] XDG Desktop Portal / PipeWire
```

---

## 2. CONNECTION HANDLING

### HTTP Server (`src/nvhttp.cpp`)

Two servers run simultaneously:
- **HTTP** (port 47989): Basic endpoints
- **HTTPS** (port 47984): Full endpoints with TLS

### Key Endpoints

| Endpoint | Purpose | Critical Line |
|----------|---------|---------------|
| `/serverinfo` | Server info | 684-774 |
| `/pair` | Pairing handshake | 552-633 |
| `/launch` | **Start streaming session** | 818-927 |
| `/resume` | Resume session | 929-1018 |
| `/cancel` | Cancel session | 1020-1043 |

### `/launch` Flow

```
1. Validate params (rikey, rikeyid, appid)
2. make_launch_session() ─── EXTRACT RESOLUTION
   └─ Parse mode="WxHxF" → width, height, fps
3. display_device::configure_display()
4. video::probe_encoders()
5. proc::proc.execute(appid, launch_session)
6. Return RTSP URL to client
7. rtsp_stream::launch_session_raise()
```

### RTSP Handshake (`src/rtsp.cpp`)

```
Moonlight → OPTIONS → Sunshine (capabilities)
Moonlight → DESCRIBE → Sunshine (codecs, encryption)
Moonlight → SETUP → Sunshine (audio/video/control ports)
Moonlight → ANNOUNCE → Sunshine *** CLIENT CONFIG ***
Moonlight → PLAY → Sunshine (start streaming)
```

### RTSP ANNOUNCE: Resolution Extraction (`rtsp.cpp:989-993`)

```cpp
config.monitor.height = args.at("x-nv-video[0].clientViewportHt");
config.monitor.width = args.at("x-nv-video[0].clientViewportWd");
config.monitor.framerate = args.at("x-nv-video[0].maxFPS");
config.monitor.bitrate = args.at("x-nv-vqos[0].bw.maximumBitrateKbps");
```

---

## 3. VIDEO CAPTURE PIPELINE

### Architecture

```
Client Resolution (config.monitor)
    │
    ▼
video::capture(mail, config, channel_data)
    │
    ├─→ capture_async() (PARALLEL_ENCODING)
    │   ├─ captureThread() - Captures frames
    │   └─ encode_run() - Encodes frames
    │
    └─→ captureThreadSync() (single thread)
```

### Capture Thread (`video.cpp:1253-1508`)

```
captureThread()
  │
  ├─ Wait for capture_ctx from queue
  ├─ refresh_displays() - Enumerate available displays
  ├─ platf::display() - Create display instance *** KEY ***
  │   └─ Returns: display_t with width/height from DISPLAY (not client!)
  ├─ Allocate image pool (12 buffers)
  │
  └─ Capture Loop:
      ├─ push_captured_image_callback - Distribute to clients
      ├─ disp->capture(push_cb, pull_cb, &cursor)
      └─ Handle reinit/error/timeout
```

### Display Abstraction (`src/platform/common.h:468-559`)

```cpp
class display_t {
    int width;      // *** DISPLAY PHYSICAL WIDTH ***
    int height;     // *** DISPLAY PHYSICAL HEIGHT ***
    int env_width;
    int env_height;
    
    virtual capture_e capture(...) = 0;
    virtual std::shared_ptr<img_t> alloc_img() = 0;
    virtual std::shared_ptr<img_t> dummy_img() = 0;
};
```

### KMS Capture (`src/platform/linux/kmsgrab.cpp`)

**Resolution Source**: `fb->width`/`fb->height` from DRM framebuffer (line 711-712)

**Capture Flow**:
```
kms::display_t::init()
  │
  ├─ Open /dev/dri/card* devices
  ├─ Enable universal planes + atomic DRM
  ├─ Find primary plane with framebuffer
  │   └─ img_width = fb->width (from hardware!)
  │   └─ img_height = fb->height (from hardware!)
  ├─ Get CRTC info for viewport
  └─ Store plane_id, crtc_id

kms::display_t::capture()
  │
  ├─ drmModeGetPlane() - Get current plane
  ├─ drmModeGetFB2() - Get framebuffer
  ├─ Convert GEM handles → DMA-BUF fds
  ├─ Import via EGL (zero-copy)
  └─ push_captured_image_callback()
```

---

## 4. ENCODER SYSTEM

### Available Encoders (Linux)

| Encoder | Codec | Line | Priority |
|---------|-------|------|----------|
| `nvenc` | NVIDIA | 518/519 | 1st |
| `vulkan` | Vulkan Video | 1019 | 2nd |
| `vaapi` | VA-API (AMD/Intel) | 961 | 3rd |
| `software` | x264/x265/SVT-AV1 | 890 | 4th |

### Encoder Selection (`probe_encoders()` - line 2794)

```
probe_encoders()
  │
  ├─ If specific encoder configured: Use that
  ├─ If HEVC/AV1 forced: Search matching encoder
  └─ Otherwise: Iterate in priority order
      └─ validate_encoder() for each:
          ├─ Create test display
          ├─ Test H.264, HEVC, AV1
          ├─ Test HDR (10-bit)
          ├─ Test YUV 4:4:4
          └─ Test reference frame invalidation
```

### VAAPI Encoder (Linux/AMD)

```
vaapi_init_avcodec_hardware_input_buffer()
  │
  ├─ Open /dev/dri/renderD128
  ├─ vaGetDisplayDRM() - Get VA display
  ├─ vaInitialize()
  ├─ av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI)
  ├─ Set VADisplay in hwctx
  └─ av_hwdevice_ctx_init()

va_t::init_codec_options()
  │
  ├─ get_va_profile() - Map FFmpeg → VA profile
  ├─ Query supported profiles
  ├─ Select entrypoint (prefer low power)
  └─ Set rate control (VBR/CBR/CQP)
```

---

## 5. RESOLUTION HANDLING

### THE CRITICAL MISMATCH

```
Moonlight requests: 1920x1080
    │
    ▼
config.monitor = {1920, 1080, 60}
    │
    ▼
captureThread() creates display:
    disp->width = 3840  (DISPLAY PHYSICAL!)
    disp->height = 2160 (DISPLAY PHYSICAL!)
    │
    ▼
make_encode_session():
    ctx->width = 1920    (from config)
    ctx->height = 1080   (from config)
    │
    ▼
SWS scaler: 3840x2160 → 1920x1080 (REDUNDANT!)
```

### Resolution Flow Summary

| Stage | Width | Height | Source |
|-------|-------|--------|--------|
| Moonlight Request | 1920 | 1080 | Client |
| RTSP ANNOUNCE | 1920 | 1080 | `config.monitor` |
| Display Instance | 3840 | 2160 | DRM framebuffer |
| Encoder Context | 1920 | 1080 | From config |
| SWS Scaling | 3840→1920 | 2160→1080 | Bridging mismatch |

### For Headless: Make Display Match Client

```
Moonlight requests: 1920x1080
    │
    ▼
HeadlessDisplay creates virtual display at 1920x1080
    │
    ▼
disp->width = 1920  (MATCHES CLIENT!)
disp->height = 1080 (MATCHES CLIENT!)
    │
    ▼
Encoder: ctx->width = 1920, ctx->height = 1080
    │
    ▼
SWS: Identity scaling (1920→1920) ─── NO QUALITY LOSS!
```

---

## 6. DISPLAY DEVICE MANAGEMENT

### `display_device` Namespace (`src/display_device.cpp`)

#### Resolution Options

```cpp
enum class resolution_option_e {
    disabled,   // Don't change resolution
    automatic,  // Use Moonlight's requested resolution ← HEADLESS TARGET
    manual      // Use manually configured resolution
};
```

#### Key Functions

| Function | Purpose | Status on Linux |
|----------|---------|-----------------|
| `configure_display()` | Apply display settings | ❌ Not implemented |
| `revert_configuration()` | Restore previous state | ❌ Not implemented |
| `parse_configuration()` | Parse config → SingleDisplayConfiguration | ✅ Works |

#### The TODO (`src/platform/linux/misc.cpp:1193`)

```cpp
// TODO(dynamic-resolution): Linux display device management entry point.
// When libdisplaydevice gains a Linux backend, resolution changes triggered by
// Moonlight's `resolution_option_e::automatic` will be routed through here.
// For now, users configure a static resolution from the web UI.
```

---

## 7. HEADLESS IMPLEMENTATION PLAN

### Problem Statement

When Sunshine runs headless (no physical monitor):
1. KMS capture fails (no DRM framebuffer)
2. Encoder probe may fail (no display for VAAPI)
3. `resolution_option_e::automatic` does nothing on Linux

### Solution Architecture

```
Moonlight connects → RTSP ANNOUNCE → config.monitor = {1920, 1080, 60}
    │
    ▼
HeadlessDisplay::create(1920, 1080, 60)
    │
    ├─ Load vkms kernel module
    ├─ Create virtual DRM connector at 1920x1080
    ├─ Create framebuffer
    └─ Return virtual display
    │
    ▼
kms::display_t::init() finds virtual display
    │
    └─ Capture from virtual display (1920x1080)
    │
    ▼
Encoder: 1920x1080 (no scaling needed!)
    │
    ▼
Stream to Moonlight
```

### Files to Create/Modify

| File | Action | Purpose |
|------|--------|---------|
| `src/platform/linux/headless.h` | NEW | HeadlessDisplay class |
| `src/platform/linux/headless.cpp` | NEW | VKMS implementation |
| `src/platform/linux/misc.cpp` | MODIFY | Add headless source detection |
| `src/platform/linux/kmsgrab.cpp` | MODIFY | Support virtual display |
| `src/video.cpp` | MODIFY | Pass client resolution to capture |
| `src/config.h` | MODIFY | Add headless config |

### Implementation Order

1. **HeadlessDisplay class** - VKMS virtual display creation
2. **Detection** - Detect when no physical monitor exists
3. **Integration** - Route to HeadlessDisplay in misc.cpp
4. **Resolution** - Ensure virtual display uses client resolution
5. **Cleanup** - Destroy virtual display on session end

---

## KEY INSIGHT FOR MIDNIGHTSUN

The **critical insight** is that Sunshine's capture resolution comes from the **physical display**, not from the client request. For headless operation, we need to create a virtual display whose resolution **matches** what the client requested, eliminating the scaling mismatch.

**Before (physical display):**
```
Client: 1920x1080 → Display: 3840x2160 → Encoder: 1920x1080 → SWS scales
```

**After (headless with MidnightSun):**
```
Client: 1920x1080 → Virtual Display: 1920x1080 → Encoder: 1920x1080 → No scaling!
```

This approach:
- Eliminates redundant scaling (better quality)
- Works without physical monitor
- Uses VKMS kernel module (available on Linux 5.x+)
- Integrates with existing KMS capture code
