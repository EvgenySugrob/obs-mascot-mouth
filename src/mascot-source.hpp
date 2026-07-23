#pragma once

#include "mouth-state.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <graphics/image-file.h>
#include <obs-audio-controls.h>
#include <obs-module.h>

namespace mascot {

class MascotSource final {
public:
  MascotSource(obs_data_t *settings, obs_source_t *source);
  ~MascotSource();

  MascotSource(const MascotSource &) = delete;
  MascotSource &operator=(const MascotSource &) = delete;

  void update(obs_data_t *settings);
  void tick(float seconds);
  void render(gs_effect_t *effect);
  [[nodiscard]] uint32_t width() const;
  [[nodiscard]] uint32_t height() const;

  static obs_properties_t *properties(void *data);
  static void defaults(obs_data_t *settings);
  static obs_source_info source_info();

private:
  struct Image {
    gs_image_file file{};
    bool initialized = false;
    std::string path;
  };

  static void meter_updated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
                            const float peak[MAX_AUDIO_CHANNELS],
                            const float input_peak[MAX_AUDIO_CHANNELS]);
  static bool enum_audio_sources(void *data, obs_source_t *source);

  void attach_meter(const char *source_name);
  void load_image(Image &image, const char *path);
  void free_image(Image &image);
  static gs_image_file *image_file(Image &image) noexcept { return &image.file; }
  static const gs_image_file *image_file(const Image &image) noexcept { return &image.file; }

  obs_source_t *source_ = nullptr;
  obs_volmeter_t *meter_ = nullptr;
  std::atomic<float> latest_level_db_{-96.0F};
  std::atomic<int> meter_channels_{0};
  std::atomic<bool> render_open_{false};
  std::mutex state_mutex_;
  MouthState mouth_{};

  mutable std::mutex image_mutex_;
  Image closed_{};
  Image open_{};
};

}
