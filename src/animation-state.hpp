#pragma once

namespace mascot {

struct AnimationSettings {
  bool speech_enabled = true;
  float bounce_pixels = 4.0F;
  float stretch_percent = 1.5F;
  float tilt_degrees = 1.5F;
  bool idle_enabled = true;
  float idle_amount_percent = 0.6F;
  float idle_speed = 0.25F;
};

struct AnimationFrame {
  float offset_y = 0.0F;
  float scale_x = 1.0F;
  float scale_y = 1.0F;
  float rotation_radians = 0.0F;
};

class AnimationState final {
public:
  AnimationState() noexcept;
  explicit AnimationState(AnimationSettings settings) noexcept;

  void configure(AnimationSettings settings) noexcept;
  void reset() noexcept;
  void update(float activity, bool emphasis, float elapsed_ms) noexcept;

  [[nodiscard]] const AnimationSettings &settings() const noexcept;
  [[nodiscard]] const AnimationFrame &frame() const noexcept;

private:
  AnimationSettings settings_{};
  AnimationFrame frame_{};
  float speech_activity_ = 0.0F;
  float speech_impulse_ = 0.0F;
  float speech_phase_ = 0.0F;
  float idle_phase_ = 0.0F;
};

}
