#pragma once

#include "animation-state.hpp"
#include "blink-state.hpp"
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

  struct ImageSet {
    Image closed{};
    Image medium{};
    Image open{};
    Image closed_blink{};
    Image medium_blink{};
    Image open_blink{};
  };

  static void meter_updated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
                            const float peak[MAX_AUDIO_CHANNELS],
                            const float input_peak[MAX_AUDIO_CHANNELS]);
  static bool enum_audio_sources(void *data, obs_source_t *source);
  void attach_meter(const char *source_name);
  void load_image(Image &image, const char *path);
  void free_image(Image &image);
  void free_image_set(ImageSet &set);
  static Image &normal_image(ImageSet &set, MouthLevel level) noexcept;
  static Image &blink_image(ImageSet &set, MouthLevel level) noexcept;
  static uint32_t image_set_width(const ImageSet &set) noexcept;
  static uint32_t image_set_height(const ImageSet &set) noexcept;
  static gs_image_file *image_file(Image &image) noexcept { return &image.file; }
  static const gs_image_file *image_file(const Image &image) noexcept { return &image.file; }

  obs_source_t *source_ = nullptr;
  obs_volmeter_t *meter_ = nullptr;
  std::atomic<float> latest_level_db_{-96.0F};
  std::atomic<int> meter_channels_{0};
  std::atomic<int> render_mouth_level_{0};
  std::atomic<bool> render_blink_{false};
  std::atomic<float> render_offset_y_{0.0F};
  std::atomic<float> render_scale_x_{1.0F};
  std::atomic<float> render_scale_y_{1.0F};
  std::atomic<float> render_rotation_{0.0F};
  bool blinking_enabled_ = true;
  MouthLevel previous_mouth_level_ = MouthLevel::Closed;
  std::mutex state_mutex_;
  MouthState mouth_{};
  BlinkState blink_{};
  AnimationState animation_{};

  mutable std::mutex image_mutex_;
  ImageSet images_{};
};

}
