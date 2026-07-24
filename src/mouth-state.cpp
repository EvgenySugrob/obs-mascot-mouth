#include "mouth-state.hpp"

#include <algorithm>
#include <cmath>

namespace mascot {

MouthState::MouthState(MouthSettings settings) : settings_(sanitize(settings)) {}

MouthSettings MouthState::sanitize(MouthSettings settings) noexcept
{
  if (!std::isfinite(settings.open_threshold_db))
    settings.open_threshold_db = -35.0F;
  if (!std::isfinite(settings.close_threshold_db))
    settings.close_threshold_db = -42.0F;

  settings.open_threshold_db = std::clamp(settings.open_threshold_db, -96.0F, 0.0F);
  settings.close_threshold_db = std::clamp(settings.close_threshold_db, -96.0F, 0.0F);
  if (settings.close_threshold_db > settings.open_threshold_db)
    settings.close_threshold_db = settings.open_threshold_db;

  if (!std::isfinite(settings.wide_threshold_db))
    settings.wide_threshold_db = -22.0F;
  settings.wide_threshold_db = std::clamp(settings.wide_threshold_db, -96.0F, 0.0F);
  if (settings.wide_threshold_db < settings.open_threshold_db)
    settings.wide_threshold_db = settings.open_threshold_db;

  if (!std::isfinite(settings.close_delay_ms))
    settings.close_delay_ms = 120.0F;
  settings.close_delay_ms = std::clamp(settings.close_delay_ms, 0.0F, 2000.0F);

  if (!std::isfinite(settings.smoothing_ms))
    settings.smoothing_ms = 80.0F;
  settings.smoothing_ms = std::clamp(settings.smoothing_ms, 0.0F, 1000.0F);

  if (!std::isfinite(settings.minimum_state_ms))
    settings.minimum_state_ms = 80.0F;
  settings.minimum_state_ms = std::clamp(settings.minimum_state_ms, 0.0F, 500.0F);
  return settings;
}

void MouthState::configure(MouthSettings settings)
{
  settings_ = sanitize(settings);
  quiet_time_ms_ = 0.0F;
}

void MouthState::reset() noexcept
{
  level_ = MouthLevel::Closed;
  quiet_time_ms_ = 0.0F;
  state_time_ms_ = settings_.minimum_state_ms;
  smoothed_level_db_ = -96.0F;
}

void MouthState::update(float level_db, float elapsed_ms) noexcept
{
  if (!std::isfinite(level_db))
    level_db = -96.0F;
  if (!std::isfinite(elapsed_ms) || elapsed_ms < 0.0F)
    elapsed_ms = 0.0F;

  if (settings_.smoothing_ms <= 0.0F) {
    smoothed_level_db_ = level_db;
  } else {
    const float smoothing =
        level_db > smoothed_level_db_ ? settings_.smoothing_ms * 0.25F : settings_.smoothing_ms;
    const float alpha = 1.0F - std::exp(-elapsed_ms / std::max(smoothing, 1.0F));
    smoothed_level_db_ += (level_db - smoothed_level_db_) * alpha;
  }

  state_time_ms_ += elapsed_ms;

  if (level_ == MouthLevel::Closed) {
    if (smoothed_level_db_ >= settings_.open_threshold_db &&
        state_time_ms_ >= settings_.minimum_state_ms)
      set_level(MouthLevel::Medium);
    return;
  }

  if (level_ == MouthLevel::Wide) {
    if (smoothed_level_db_ <= settings_.wide_threshold_db - 3.0F &&
        state_time_ms_ >= settings_.minimum_state_ms)
      set_level(MouthLevel::Medium);
    return;
  }

  if (smoothed_level_db_ >= settings_.wide_threshold_db &&
      state_time_ms_ >= settings_.minimum_state_ms) {
    set_level(MouthLevel::Wide);
    return;
  }

  if (smoothed_level_db_ > settings_.close_threshold_db) {
    quiet_time_ms_ = 0.0F;
    return;
  }

  quiet_time_ms_ += elapsed_ms;
  if (quiet_time_ms_ >= settings_.close_delay_ms) {
    set_level(MouthLevel::Closed);
  }
}

float MouthState::activity() const noexcept
{
  if (level_ == MouthLevel::Closed)
    return 0.0F;

  const float range = settings_.wide_threshold_db - settings_.open_threshold_db;
  if (range <= 0.0F)
    return 1.0F;
  return std::clamp((smoothed_level_db_ - settings_.open_threshold_db) / range, 0.25F, 1.0F);
}

void MouthState::set_level(MouthLevel level) noexcept
{
  level_ = level;
  quiet_time_ms_ = 0.0F;
  state_time_ms_ = 0.0F;
}

}
