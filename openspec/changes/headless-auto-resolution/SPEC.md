# SPECIFICATIONS: Headless Auto-Resolution for MidnightSun

## Change ID: headless-auto-resolution
## Version: 1.0
## Date: 2026-04-09

---

## 1. SCOPE

These specs cover MidnightSun running headless (no physical monitor) on Linux with AMD GPU, receiving resolution requests from Moonlight clients.

---

## 2. SCENARIOS

### Scenario 1: Headless Launch with Moonlight Requesting 1080p

**Given** MidnightSun is running on Linux without a physical monitor connected
**And** no virtual display is configured
**And** the Moonlight client requests 1920x1080@60fps
**When** the client initiates a stream session
**Then** MidnightSun SHALL create a virtual display at 1920x1080
**And** the capture pipeline SHALL capture at 1920x1080
**And** the encoder SHALL output at 1920x1080
**And** the stream SHALL work without errors

### Scenario 2: Headless Launch with Moonlight Requesting 4K

**Given** MidnightSun is running on Linux without a physical monitor connected
**And** the Moonlight client requests 3840x2160@60fps
**When** the client initiates a stream session
**Then** MidnightSun SHALL create a virtual display at 3840x2160
**And** the capture pipeline SHALL capture at 3840x2160
**And** the encoder SHALL output at 3840x2160

### Scenario 3: Normal Operation with Physical Monitor (Regression Prevention)

**Given** MidnightSun is running on Linux with a physical monitor connected
**And** the monitor is at native resolution (e.g., 2560x1440)
**And** the Moonlight client requests 1920x1080
**When** the client initiates a stream session
**Then** MidnightSun SHALL use the existing capture behavior
**And** headless virtual display SHALL NOT be created
**And** capture SHALL use the physical monitor's resolution
**And** encoder SHALL downscale/upscale to client's requested resolution

### Scenario 4: Transition from Physical Monitor to Headless

**Given** MidnightSun is streaming with a physical monitor
**And** the physical monitor is disconnected
**When** MidnightSun detects the disconnection
**Then** MidnightSun SHOULD maintain the current stream session
**And** on next session, MidnightSun SHALL use virtual display fallback

### Scenario 5: VKMS Module Not Available

**Given** MidnightSun is running on Linux without a physical monitor
**And** the vkms kernel module is NOT available
**When** the client initiates a stream session
**Then** MidnightSun SHALL attempt software-based capture fallback
**And** MidnightSun SHALL log a warning about vkms unavailability
**And** the stream SHALL still work (with possible performance implications)

### Scenario 6: Resolution Changes Between Sessions

**Given** MidnightSun has a virtual display created at 1920x1080
**And** the previous session has ended
**And** a new Moonlight client requests 2560x1440
**When** the new session starts
**Then** MidnightSun SHALL destroy the previous virtual display
**And** MidnightSun SHALL create a new virtual display at 2560x1440

### Scenario 7: Multiple Virtual Display Resolution Support

**Given** MidnightSun supports the following Moonlight resolutions:
  | Resolution | Support |
  |-----------|---------|
  | 1280x720  | MUST |
  | 1920x1080 | MUST |
  | 2560x1440 | MUST |
  | 3840x2160 | MUST |
**When** a client requests any supported resolution
**Then** MidnightSun SHALL create a virtual display at that resolution

---

## 3. REQUIREMENTS

### Functional Requirements

| ID | Requirement | Priority |
|----|------------|----------|
| FR-1 | Detect headless state (no physical monitor connected) | MUST |
| FR-2 | Create virtual display using VKMS when headless | MUST |
| FR-3 | Pass client-requested resolution to capture pipeline | MUST |
| FR-4 | Destroy virtual display when session ends | MUST |
| FR-5 | Fallback to software capture if VKMS unavailable | SHOULD |
| FR-6 | Support all Moonlight-requested resolutions | MUST |
| FR-7 | Preserve existing behavior when physical monitor exists | MUST |

### Non-Functional Requirements

| ID | Requirement | Priority |
|----|------------|----------|
| NFR-1 | Virtual display creation latency < 500ms | MUST |
| NFR-2 | No degradation of normal capture performance | MUST |
| NFR-3 | Works on Linux kernel 5.x+ (VKMS available) | MUST |
| NFR-4 | Works on Bazzite OS with AMD GPU | MUST |
| NFR-5 | Graceful error handling when VKMS fails | MUST |

---

## 4. ACCEPTANCE CRITERIA

- [ ] MidnightSun starts without error on headless Linux
- [ ] Moonlight can connect and request any resolution
- [ ] Stream quality matches requested resolution
- [ ] No regression in normal (with monitor) operation
- [ ] Proper cleanup of virtual display on session end
- [ ] Logging shows clear headless mode activation

---

## 5. KEYWORDS

- `MUST`, `SHALL`: Required for implementation
- `SHOULD`: Recommended but not required
- `MAY`: Optional
