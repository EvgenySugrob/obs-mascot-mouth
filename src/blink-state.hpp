#pragma once

#include <cstdint>

namespace mascot {

struct BlinkSettings {
  float minimum_interval_ms = 3500.0F;
  float maximum_interval_ms = 6500.0F;
  float duration_ms = 120.0F;
};

class BlinkState final {
public:
  explicit BlinkState(BlinkSettings settings = {}, std::uint32_t seed = 0x9E3779B9U);

  void configure(BlinkSettings settings);
  void reset() noexcept;
  void update(float elapsed_ms) noexcept;

  [[nodiscard]] bool is_blinking() const noexcept { return blinking_; }
  [[nodiscard]] const BlinkSettings &settings() const noexcept { return settings_; }

private:
  static BlinkSettings sanitize(BlinkSettings settings) noexcept;
  float next_interval() noexcept;

  BlinkSettings settings_{};
  std::uint32_t random_state_ = 0;
  bool blinking_ = false;
  float phase_remaining_ms_ = 0.0F;
};

}
