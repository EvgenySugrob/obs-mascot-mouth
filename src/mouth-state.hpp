#pragma once

namespace mascot {

struct MouthSettings {
  float open_threshold_db = -35.0F;
  float close_threshold_db = -42.0F;
  float close_delay_ms = 120.0F;
};

class MouthState final {
public:
  explicit MouthState(MouthSettings settings = {});

  void configure(MouthSettings settings);
  void reset() noexcept;
  void update(float level_db, float elapsed_ms) noexcept;

  [[nodiscard]] bool is_open() const noexcept { return open_; }
  [[nodiscard]] const MouthSettings &settings() const noexcept { return settings_; }

private:
  static MouthSettings sanitize(MouthSettings settings) noexcept;

  MouthSettings settings_{};
  bool open_ = false;
  float quiet_time_ms_ = 0.0F;
};

}
