/**
 * @file src/platform/linux/headless.cpp
 * @brief Implementation for headless virtual display via VKMS.
 */

// standard includes
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

// platform includes
#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// local includes
#include "headless.h"
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/utility.h"

using namespace std::literals;
namespace fs = std::filesystem;

namespace platf::headless {

  /**
   * @brief RAII wrapper for CAP_SYS_ADMIN capability.
   */
  class cap_sys_admin {
  public:
    cap_sys_admin() {
      caps = cap_get_proc();

      cap_value_t sys_admin = CAP_SYS_ADMIN;
      if (cap_set_flag(caps, CAP_EFFECTIVE, 1, &sys_admin, CAP_SET) || cap_set_proc(caps)) {
        BOOST_LOG(error) << "Failed to gain CAP_SYS_ADMIN"sv;
      }
    }

    ~cap_sys_admin() {
      cap_value_t sys_admin = CAP_SYS_ADMIN;
      if (cap_set_flag(caps, CAP_EFFECTIVE, 1, &sys_admin, CAP_CLEAR) || cap_set_proc(caps)) {
        BOOST_LOG(error) << "Failed to drop CAP_SYS_ADMIN"sv;
      }
      cap_free(caps);
    }

    cap_t caps;
  };

  /**
   * @brief Check if VKMS module is loaded.
   * @return true if VKMS module is present in /sys/module
   */
  static bool is_vkms_loaded() {
    return fs::exists("/sys/module/vkms"sv);
  }

  /**
   * @brief Attempt to load the VKMS kernel module.
   * @return true if module loaded successfully
   */
  static bool load_vkms_module() {
    if (is_vkms_loaded()) {
      return true;
    }

    BOOST_LOG(info) << "Attempting to load VKMS kernel module"sv;

    cap_sys_admin admin;
    int ret = system("modprobe vkms 2>/dev/null"sv.data());
    if (ret != 0) {
      BOOST_LOG(warning) << "Failed to load VKMS module via modprobe"sv;
      return false;
    }

    // Wait a moment for the module to initialize
    usleep(100000);

    return is_vkms_loaded();
  }

  /**
   * @brief Find VKMS card device path.
   * @return Path to VKMS card device, or empty string if not found
   */
  static std::string find_vkms_card() {
    fs::path card_dir {"/dev/dri"sv};

    for (auto &entry : fs::directory_iterator {card_dir}) {
      auto file = entry.path().filename().generic_string();
      if (file.size() < 4 || std::string_view {file}.substr(0, 4) != "card"sv) {
        continue;
      }

      // Open the card and check if it's VKMS
      cap_sys_admin admin;
      int fd = open(entry.path().c_str(), O_RDWR);
      if (fd < 0) {
        continue;
      }

      util::safe_ptr<drmVersion, drmFreeVersion> ver {drmGetVersion(fd)};
      if (ver && ver->name) {
        std::string_view driver_name {ver->name, static_cast<size_t>(ver->name_len)};
        if (driver_name == "vkms"sv) {
          BOOST_LOG(info) << "Found VKMS card at: "sv << entry.path();
          close(fd);
          return entry.path().string();
        }
      }

      close(fd);
    }

    return {};
  }

  bool HeadlessDisplay::is_vkms_available() {
    // Check if VKMS is already loaded
    if (is_vkms_loaded()) {
      return true;
    }

    // Try to load it
    return load_vkms_module();
  }

  std::unique_ptr<HeadlessDisplay> HeadlessDisplay::create(
    int width, int height, int refresh_rate) {

    auto display = std::unique_ptr<HeadlessDisplay>(new HeadlessDisplay());

    // Try VKMS first
    if (display->setup_vkms(width, height, refresh_rate)) {
      BOOST_LOG(info) << "Created headless virtual display via VKMS: "sv << width << "x"sv << height
                      << "@"sv << refresh_rate << "Hz"sv;
      return display;
    }

    // VKMS failed - fall back to software mode
    BOOST_LOG(warning) << "VKMS unavailable, falling back to software capture mode"sv;

    if (!display->setup_software(width, height, refresh_rate)) {
      BOOST_LOG(error) << "Failed to create headless display at "sv << width << "x"sv << height;
      return nullptr;
    }

    BOOST_LOG(warning) << "Created headless display in SOFTWARE mode: "sv << width << "x"sv << height
                       << "@"sv << refresh_rate << "Hz (performance may be reduced)"sv;
    return display;
  }

  bool HeadlessDisplay::setup_vkms(int w, int h, int refresh) {
    // Ensure VKMS is available
    if (!is_vkms_available()) {
      BOOST_LOG(error) << "VKMS module not available"sv;
      return false;
    }

    // Find VKMS card
    auto card_path = find_vkms_card();
    if (card_path.empty()) {
      BOOST_LOG(error) << "No VKMS card found"sv;
      return false;
    }

    cap_sys_admin admin;

    // Open the VKMS card
    drm_fd_ = open(card_path.c_str(), O_RDWR | O_CLOEXEC);
    if (drm_fd_ < 0) {
      BOOST_LOG(error) << "Failed to open VKMS card: "sv << strerror(errno);
      return false;
    }

    // Enable universal planes for access to all plane types
    if (drmSetClientCap(drm_fd_, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
      BOOST_LOG(warning) << "Failed to enable universal planes capability"sv;
    }

    // Enable atomic mode-setting if available
    if (drmSetClientCap(drm_fd_, DRM_CLIENT_CAP_ATOMIC, 1)) {
      BOOST_LOG(debug) << "Atomic mode-setting not available, using legacy API"sv;
    }

    // Get DRM resources
    auto resources = util::safe_ptr<drmModeRes, drmModeFreeResources>(drmModeGetResources(drm_fd_));
    if (!resources) {
      BOOST_LOG(error) << "Failed to get DRM resources: "sv << strerror(errno);
      cleanup();
      return false;
    }

    // Find a suitable connector (preferably disconnected for virtual use)
    uint32_t conn_id = 0;
    for (int i = 0; i < resources->count_connectors; ++i) {
      auto conn = util::safe_ptr<drmModeConnector, drmModeFreeConnector>(
        drmModeGetConnector(drm_fd_, resources->connectors[i]));
      if (conn) {
        conn_id = conn->connector_id;
        // Use the first available connector
        break;
      }
    }

    if (!conn_id) {
      BOOST_LOG(error) << "No suitable connector found on VKMS card"sv;
      cleanup();
      return false;
    }

    connector_id_ = conn_id;

    // Find a suitable encoder
    auto conn = util::safe_ptr<drmModeConnector, drmModeFreeConnector>(
      drmModeGetConnector(drm_fd_, connector_id_));
    if (conn && conn->count_encoders > 0) {
      encoder_id_ = conn->encoders[0];
    }

    // Find a suitable CRTC
    if (encoder_id_) {
      auto enc = util::safe_ptr<drmModeEncoder, drmModeFreeEncoder>(
        drmModeGetEncoder(drm_fd_, encoder_id_));
      if (enc) {
        crtc_id_ = enc->crtc_id;
      }
    }

    // If no CRTC from encoder, find any available CRTC
    if (!crtc_id_ && resources->count_crtcs > 0) {
      crtc_id_ = resources->crtcs[0];
    }

    if (!crtc_id_) {
      BOOST_LOG(error) << "No suitable CRTC found"sv;
      cleanup();
      return false;
    }

    // Create a dumb buffer for the framebuffer
    struct drm_mode_create_dumb create_dumb = {};
    create_dumb.width = w;
    create_dumb.height = h;
    create_dumb.bpp = 32;  // ARGB8888

    if (drmIoctl(drm_fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) < 0) {
      BOOST_LOG(error) << "Failed to create dumb buffer: "sv << strerror(errno);
      cleanup();
      return false;
    }

    gem_handle_ = create_dumb.handle;

    // Create framebuffer from dumb buffer
    uint32_t pitches[4] = {create_dumb.pitch};
    uint32_t offsets[4] = {0};
    uint32_t handles[4] = {gem_handle_};

    if (drmModeAddFB2(drm_fd_, w, h, DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &framebuffer_id_, 0) < 0) {
      // Fall back to legacy AddFB
      if (drmModeAddFB(drm_fd_, w, h, 24, 32, create_dumb.pitch,
                       gem_handle_, &framebuffer_id_) < 0) {
        BOOST_LOG(error) << "Failed to create framebuffer: "sv << strerror(errno);
        cleanup();
        return false;
      }
    }

    // Find a primary plane
    auto plane_res = util::safe_ptr<drmModePlaneRes, drmModeFreePlaneResources>(
      drmModeGetPlaneResources(drm_fd_));
    if (plane_res) {
      for (uint32_t i = 0; i < plane_res->count_planes; ++i) {
        auto plane = util::safe_ptr<drmModePlane, drmModeFreePlane>(
          drmModeGetPlane(drm_fd_, plane_res->planes[i]));
        if (plane && plane->possible_crtcs) {
          // Check if this plane can be used with our CRTC
          int crtc_idx = -1;
          for (int j = 0; j < resources->count_crtcs; ++j) {
            if (resources->crtcs[j] == crtc_id_) {
              crtc_idx = j;
              break;
            }
          }

          if (crtc_idx >= 0 && (plane->possible_crtcs & (1 << crtc_idx))) {
            plane_id_ = plane->plane_id;
            BOOST_LOG(debug) << "Found primary plane: "sv << plane_id_;
            break;
          }
        }
      }
    }

    // Set the mode
    drmModeModeInfo mode = {};
    mode.clock = (w * h * refresh) / 1000;
    mode.hdisplay = w;
    mode.hsync_start = w + 48;
    mode.hsync_end = w + 48 + 32;
    mode.htotal = w + 48 + 32 + 80;
    mode.vdisplay = h;
    mode.vsync_start = h + 3;
    mode.vsync_end = h + 3 + 5;
    mode.vtotal = h + 3 + 5 + 24;
    mode.vrefresh = refresh;
    mode.type = DRM_MODE_TYPE_PREFERRED;
    snprintf(mode.name, DRM_DISPLAY_MODE_LEN, "%dx%d@%d", w, h, refresh);

    if (drmModeSetCrtc(drm_fd_, crtc_id_, framebuffer_id_, 0, 0,
                       &connector_id_, 1, &mode) < 0) {
      BOOST_LOG(error) << "Failed to set CRTC mode: "sv << strerror(errno);
      cleanup();
      return false;
    }

    width_ = w;
    height_ = h;
    refresh_rate_ = refresh;
    valid_ = true;

    return true;
  }

  bool HeadlessDisplay::setup_software(int w, int h, int refresh) {
    width_ = w;
    height_ = h;
    refresh_rate_ = refresh;
    software_mode_ = true;
    valid_ = true;

    BOOST_LOG(info) << "Software fallback display initialized: "sv << w << "x"sv << h
                    << "@"sv << refresh << "Hz (no DRM resources)"sv;
    return true;
  }

  void HeadlessDisplay::cleanup() {
    if (drm_fd_ >= 0) {
      cap_sys_admin admin;

      if (framebuffer_id_) {
        drmModeRmFB(drm_fd_, framebuffer_id_);
        framebuffer_id_ = 0;
      }

      if (gem_handle_) {
        struct drm_gem_close close_args = {};
        close_args.handle = gem_handle_;
        drmIoctl(drm_fd_, DRM_IOCTL_GEM_CLOSE, &close_args);
        gem_handle_ = 0;
      }

      close(drm_fd_);
      drm_fd_ = -1;
    }

    valid_ = false;
  }

  HeadlessDisplay::~HeadlessDisplay() {
    cleanup();
  }

  bool HeadlessDisplay::is_valid() const {
    return valid_;
  }

  int HeadlessDisplay::get_drm_fd() const {
    return drm_fd_;
  }

  uint32_t HeadlessDisplay::get_connector_id() const {
    return connector_id_;
  }

  uint32_t HeadlessDisplay::get_crtc_id() const {
    return crtc_id_;
  }

  uint32_t HeadlessDisplay::get_framebuffer_id() const {
    return framebuffer_id_;
  }

  uint32_t HeadlessDisplay::get_plane_id() const {
    return plane_id_;
  }

  int HeadlessDisplay::get_width() const {
    return width_;
  }

  int HeadlessDisplay::get_height() const {
    return height_;
  }

  int HeadlessDisplay::get_refresh_rate() const {
    return refresh_rate_;
  }

  bool HeadlessDisplay::is_software_mode() const {
    return software_mode_;
  }

  bool is_headless() {
    // Check for connected physical displays via KMS
    fs::path card_dir {"/dev/dri"sv};

    for (auto &entry : fs::directory_iterator {card_dir}) {
      auto file = entry.path().filename().generic_string();
      if (file.size() < 4 || std::string_view {file}.substr(0, 4) != "card"sv) {
        continue;
      }

      int fd = open(entry.path().c_str(), O_RDWR | O_CLOEXEC);
      if (fd < 0) {
        continue;
      }

      // Skip VKMS cards when checking for physical displays
      util::safe_ptr<drmVersion, drmFreeVersion> ver {drmGetVersion(fd)};
      if (ver && ver->name) {
        std::string_view driver_name {ver->name, static_cast<size_t>(ver->name_len)};
        if (driver_name == "vkms"sv) {
          close(fd);
          continue;
        }
      }

      auto resources = util::safe_ptr<drmModeRes, drmModeFreeResources>(drmModeGetResources(fd));
      if (resources) {
        for (int i = 0; i < resources->count_connectors; ++i) {
          auto conn = util::safe_ptr<drmModeConnector, drmModeFreeConnector>(
            drmModeGetConnector(fd, resources->connectors[i]));
          if (conn && conn->connection == DRM_MODE_CONNECTED) {
            close(fd);
            return false;  // Found a connected physical display
          }
        }
      }

      close(fd);
    }

    return true;  // No connected physical displays found
  }

  std::vector<std::string> headless_display_names() {
    return {"headless"sv};
  }

  /**
   * @brief Display implementation for headless virtual displays.
   */
  class headless_display_t: public platf::display_t {
  public:
    headless_display_t() = default;

    int init(const std::string &display_name, const ::video::config_t &config) {
      // Create a virtual display at the requested resolution
      virtual_display_ = HeadlessDisplay::create(config.width, config.height, config.framerate);
      if (!virtual_display_ || !virtual_display_->is_valid()) {
        BOOST_LOG(error) << "Failed to create headless display"sv;
        return -1;
      }

      if (virtual_display_->is_software_mode()) {
        BOOST_LOG(warning) << "Headless display running in SOFTWARE FALLBACK mode"sv;
        BOOST_LOG(warning) << "  - VKMS kernel module is not available"sv;
        BOOST_LOG(warning) << "  - Streaming will work but with reduced capture performance"sv;
        BOOST_LOG(warning) << "  - Consider installing VKMS: modprobe vkms"sv;
      }

      // Set display dimensions
      width = config.width;
      height = config.height;
      offset_x = 0;
      offset_y = 0;
      env_width = config.width;
      env_height = config.height;
      logical_width = config.width;
      logical_height = config.height;
      env_logical_width = config.width;
      env_logical_height = config.height;

      delay = std::chrono::nanoseconds {1s} / config.framerate;

      return 0;
    }

    capture_e capture(const push_captured_image_cb_t &push_captured_image_cb,
                      const pull_free_image_cb_t &pull_free_image_cb,
                      bool *cursor) override {
      auto next_frame = std::chrono::steady_clock::now();

      while (true) {
        auto now = std::chrono::steady_clock::now();

        if (next_frame > now) {
          std::this_thread::sleep_for(next_frame - now);
        }

        next_frame += delay;
        if (next_frame < now) {
          next_frame = now + delay;
        }

        // For headless display, we produce a blank frame
        // This allows the streaming pipeline to work even without real content
        std::shared_ptr<platf::img_t> img_out;
        if (!pull_free_image_cb(img_out)) {
          return platf::capture_e::interrupted;
        }

        // Clear the frame buffer (black screen)
        if (img_out && img_out->data) {
          std::memset(img_out->data, 0, img_out->height * img_out->row_pitch);
        }

        if (!push_captured_image_cb(std::move(img_out), true)) {
          return platf::capture_e::ok;
        }
      }

      return capture_e::ok;
    }

    std::shared_ptr<img_t> alloc_img() override {
      auto img = std::make_shared<img_t>();
      img->width = width;
      img->height = height;
      img->pixel_pitch = 4;
      img->row_pitch = img->pixel_pitch * width;
      img->data = new std::uint8_t[height * img->row_pitch];
      return img;
    }

    int dummy_img(img_t *img) override {
      return 0;
    }

    ~headless_display_t() override = default;

  private:
    std::unique_ptr<HeadlessDisplay> virtual_display_;
    std::chrono::nanoseconds delay;
  };

}  // namespace platf::headless

namespace platf {

  std::shared_ptr<display_t> headless_display(mem_type_e hwdevice_type, const std::string &display_name, const video::config_t &config) {
    auto disp = std::make_shared<headless::headless_display_t>();

    if (disp->init(display_name, config)) {
      return nullptr;
    }

    return disp;
  }

}  // namespace platf
