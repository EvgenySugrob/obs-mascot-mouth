#pragma once

namespace mascot {

enum class MouthLevel {
  Closed,
  Medium,
  Wide
};

struct MouthSettings {
  float open_threshold_db = -35.0F;
  float close_threshold_db = -42.0F;
  float close_delay_ms = 120.0F;
  float wide_threshold_db = -22.0F;
  float smoothing_ms = 80.0F;
  float minimum_state_ms = 80.0F;
};

class MouthState final {
public:
  explicit MouthState(MouthSettings settings = {});

  void configure(MouthSettings settings);
  void reset() noexcept;
  void update(float level_db, float elapsed_ms) noexcept;

  [[nodiscard]] bool is_open() const noexcept { return level_ != MouthLevel::Closed; }
  [[nodiscard]] MouthLevel level() const noexcept { return level_; }
  [[nodiscard]] float activity() const noexcept;
  [[nodiscard]] float smoothed_level_db() const noexcept { return smoothed_level_db_; }
  [[nodiscard]] const MouthSettings &settings() const noexcept { return settings_; }

private:
  static MouthSettings sanitize(MouthSettings settings) noexcept;
  void set_level(MouthLevel level) noexcept;

  MouthSettings settings_{};
  MouthLevel level_ = MouthLevel::Closed;
  float quiet_time_ms_ = 0.0F;
  float state_time_ms_ = 0.0F;
  float smoothed_level_db_ = -96.0F;
};

}
