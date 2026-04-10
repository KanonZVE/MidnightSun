/**
 * @file src/platform/linux/headless.h
 * @brief Declarations for headless virtual display via VKMS.
 */
#pragma once

// standard includes
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace platf::headless {

/**
 * @brief Creates and manages a VKMS virtual display for headless operation.
 *
 * When no physical monitor is connected, this class creates a virtual display
 * using the VKMS (Virtual Kernel Mode Setting) kernel module. The virtual display
 * matches the resolution requested by the Moonlight client.
 */
class HeadlessDisplay {
public:
  /**
   * @brief Create a virtual display at the specified resolution.
   * @param width Display width in pixels
   * @param height Display height in pixels
   * @param refresh_rate Refresh rate in Hz (default: 60)
   * @return Unique pointer to HeadlessDisplay, or nullptr on failure
   */
  static std::unique_ptr<HeadlessDisplay> create(
    int width, int height, int refresh_rate = 60);

  /**
   * @brief Check if VKMS module is available.
   * @return true if VKMS module can be loaded or is already loaded
   */
  static bool is_vkms_available();

  /**
   * @brief Check if this display is valid and usable.
   * @return true if virtual display was created successfully
   */
  bool is_valid() const;

  /**
   * @brief Get the DRM file descriptor for capture.
   * @return DRM file descriptor, or -1 if invalid
   */
  int get_drm_fd() const;

  /**
   * @brief Get the connector ID for KMS capture.
   * @return DRM connector ID
   */
  uint32_t get_connector_id() const;

  /**
   * @brief Get the CRTC ID for KMS capture.
   * @return DRM CRTC ID
   */
  uint32_t get_crtc_id() const;

  /**
   * @brief Get the framebuffer ID for KMS capture.
   * @return DRM framebuffer ID
   */
  uint32_t get_framebuffer_id() const;

  /**
   * @brief Get the plane ID for KMS capture.
   * @return DRM plane ID
   */
  uint32_t get_plane_id() const;

  /**
   * @brief Get display width.
   * @return Width in pixels
   */
  int get_width() const;

  /**
   * @brief Get display height.
   * @return Height in pixels
   */
  int get_height() const;

  /**
   * @brief Get display refresh rate.
   * @return Refresh rate in Hz
   */
  int get_refresh_rate() const;

  /**
   * @brief Check if this display is running in software fallback mode.
   * @return true if VKMS is unavailable and we're using software buffers
   */
  bool is_software_mode() const;

  ~HeadlessDisplay();

private:
  HeadlessDisplay() = default;

  bool setup_vkms(int w, int h, int refresh);
  bool setup_software(int w, int h, int refresh);
  void cleanup();

  int drm_fd_ = -1;
  uint32_t connector_id_ = 0;
  uint32_t crtc_id_ = 0;
  uint32_t encoder_id_ = 0;
  uint32_t framebuffer_id_ = 0;
  uint32_t plane_id_ = 0;
  uint32_t gem_handle_ = 0;

  int width_ = 0;
  int height_ = 0;
  int refresh_rate_ = 60;

  bool valid_ = false;
  bool software_mode_ = false;
};

/**
 * @brief Check if the system is in headless mode (no physical displays).
 * @return true if no physical displays are connected
 */
bool is_headless();

/**
 * @brief Get display names for headless virtual displays.
 * @return Vector containing virtual display identifier
 */
std::vector<std::string> headless_display_names();

}  // namespace platf::headless
