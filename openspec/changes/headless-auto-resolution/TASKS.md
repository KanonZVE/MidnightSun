# TASK BREAKDOWN: Headless Auto-Resolution for Linux

## Change ID: headless-auto-resolution
## Version: 1.0
## Date: 2026-04-09

---

## PHASE 1: INFRASTRUCTURE (Week 1)

### Task 1.1: VKMS Integration Foundation ✅ COMPLETED
**Description**: Create the HeadlessDisplay class with VKMS support
**Files**: `src/platform/linux/headless.h`, `src/platform/linux/headless.cpp`
**Acceptance Criteria**:
- [x] Can detect VKMS module availability (`is_vkms_available()`)
- [x] Can load VKMS module (`modprobe vkms` with CAP_SYS_ADMIN)
- [x] Can create virtual connector/crtc/framebuffer (dumb buffer + drmModeAddFB2)
- [x] Can provide DRM file descriptor to capture pipeline (`get_drm_fd()`)
**Estimated**: 2-3 days

### Task 1.2: Configuration Extension
**Description**: Add headless configuration options
**Files**: `src/config.h`, `src/config.cpp`
**Acceptance Criteria**:
- [ ] New `headless_t` config struct
- [ ] Fallback resolution configuration
- [ ] VKMS preference option
- [ ] Config loads/saves properly
**Estimated**: 1 day

### Task 1.3: Headless Detection Logic ✅ COMPLETED
**Description**: Implement detection of "no physical display" state
**Files**: `src/platform/linux/headless.cpp`, `src/platform/linux/misc.cpp`
**Acceptance Criteria**:
- [x] Can scan DRM connectors for active displays (`is_headless()`)
- [x] Returns true when no physical display connected
- [x] Returns false when any display is connected
- [x] Works with AMD GPU (skips VKMS cards when checking for physical displays)
**Estimated**: 1 day

---

## PHASE 2: CORE IMPLEMENTATION (Week 2)

### Task 2.1: KMS Capture Integration
**Description**: Integrate HeadlessDisplay with existing KMS capture
**Files**: `src/platform/linux/kmsgrab.cpp`, `src/platform/linux/kmsgrab.h`
**Acceptance Criteria**:
- [ ] When headless detected, HeadlessDisplay is created
- [ ] KMS capture uses virtual display DRM fd
- [ ] Framebuffer from virtual display is used for capture
- [ ] Normal (with monitor) operation unchanged
**Estimated**: 2-3 days

### Task 2.2: Resolution Propagation
**Description**: Pass client-requested resolution to capture initialization
**Files**: `src/video.cpp`
**Acceptance Criteria**:
- [ ] `video::config_t` width/height accessible in capture init
- [ ] Capture uses client resolution when creating virtual display
- [ ] Existing resolution flow unchanged for physical displays
**Estimated**: 1 day

### Task 2.3: Session Lifecycle Integration
**Description**: Proper creation/cleanup of virtual display per session
**Files**: `src/video.cpp`, `src/platform/linux/headless_display.cpp`
**Acceptance Criteria**:
- [ ] Virtual display created at session start
- [ ] Virtual display destroyed at session end
- [ ] No memory leaks
- [ ] Multiple sessions work correctly
**Estimated**: 2 days

---

## PHASE 3: FALLBACK & ERROR HANDLING (Week 3)

### Task 3.1: VKMS Fallback
**Description**: Software capture when VKMS unavailable
**Files**: `src/platform/linux/headless_display.cpp`
**Acceptance Criteria**:
- [ ] Detects VKMS unavailability
- [ ] Falls back to software buffer capture
- [ ] Logs appropriate warnings
- [ ] Stream still works (with possible quality impact)
**Estimated**: 2 days

### Task 3.2: Error Handling & Logging
**Description**: Robust error handling for all failure modes
**Files**: `src/platform/linux/headless_display.cpp`, `src/platform/linux/kmsgrab.cpp`
**Acceptance Criteria**:
- [ ] Clear error messages for VKMS failure
- [ ] Graceful degradation for partial failures
- [ ] Logs show headless mode activation/deactivation
- [ ] No crashes on any error path
**Estimated**: 1 day

### Task 3.3: Gamescope Compatibility
**Description**: Ensure VKMS virtual display doesn't interfere with Gamescope
**Files**: `src/platform/linux/headless_display.cpp`
**Acceptance Criteria**:
- [ ] Virtual display uses separate device node
- [ ] Gamescope's virtual output not affected
- [ ] Tested on Steam Deck (Gamescope environment)
**Estimated**: 1 day

---

## PHASE 4: TESTING & POLISH (Week 4-5)

### Task 4.1: Manual Testing on Bazzite
**Description**: End-to-end testing on Bazzite OS with AMD GPU
**Acceptance Criteria**:
- [ ] Headless 1080p stream works
- [ ] Headless 4K stream works
- [ ] With-monitor operation unchanged
- [ ] Multiple session cycles work
- [ ] No performance regression
**Estimated**: 2-3 days

### Task 4.2: Unit Tests (Limited)
**Description**: Unit tests for non-hardware-dependent code
**Acceptance Criteria**:
- [ ] HeadlessDisplay creation logic tested
- [ ] Configuration parsing tested
- [ ] Resolution propagation tested
- [ ] Error paths tested
**Estimated**: 1-2 days

### Task 4.3: Documentation
**Description**: Update documentation with headless mode info
**Acceptance Criteria**:
- [ ] README updated with headless section
- [ ] Configuration options documented
- [ ] VKMS requirements documented
- [ ] Troubleshooting section added
**Estimated**: 1 day

### Task 4.4: Code Review & Cleanup
**Description**: Final code review and cleanup
**Acceptance Criteria**:
- [ ] Code follows existing style
- [ ] No unused code
- [ ] Comments explain complex logic
- [ ] Ready for PR/merge
**Estimated**: 1 day

---

## DEPENDENCY GRAPH

```
Phase 1 (Infrastructure)
├── 1.1 VKMS Foundation ─────┐
├── 1.2 Config Extension ────┼──→ Phase 2
└── 1.3 Headless Detection ──┘

Phase 2 (Core Implementation)
├── 2.1 KMS Integration ←── 1.1, 1.3
├── 2.2 Resolution Propagation ←── 1.2
└── 2.3 Session Lifecycle ←── 2.1, 2.2

Phase 3 (Fallback)
├── 3.1 VKMS Fallback ←── 1.1
├── 3.2 Error Handling ←── 2.1, 2.3
└── 3.3 Gamescope ←── 2.1

Phase 4 (Testing)
├── 4.1 Manual Testing ←── 2.1, 2.2, 2.3, 3.1, 3.2
├── 4.2 Unit Tests ←── 1.1, 1.2, 2.2
├── 4.3 Documentation ←── 4.1
└── 4.4 Cleanup ←── 4.1, 4.2, 4.3
```

---

## TOTAL ESTIMATE

| Phase | Tasks | Days |
|-------|-------|------|
| Phase 1 | 3 | 5 |
| Phase 2 | 3 | 6 |
| Phase 3 | 3 | 4 |
| Phase 4 | 4 | 6 |
| **Total** | **13** | **~21 days** |

Conservative estimate: 3-4 weeks of development
