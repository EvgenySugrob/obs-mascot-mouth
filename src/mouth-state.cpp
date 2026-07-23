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

  if (!std::isfinite(settings.close_delay_ms))
    settings.close_delay_ms = 120.0F;
  settings.close_delay_ms = std::clamp(settings.close_delay_ms, 0.0F, 2000.0F);
  return settings;
}

void MouthState::configure(MouthSettings settings)
{
  settings_ = sanitize(settings);
  quiet_time_ms_ = 0.0F;
}

void MouthState::reset() noexcept
{
  open_ = false;
  quiet_time_ms_ = 0.0F;
}

void MouthState::update(float level_db, float elapsed_ms) noexcept
{
  if (!std::isfinite(level_db))
    level_db = -96.0F;
  if (!std::isfinite(elapsed_ms) || elapsed_ms < 0.0F)
    elapsed_ms = 0.0F;

  if (!open_) {
    if (level_db >= settings_.open_threshold_db) {
      open_ = true;
      quiet_time_ms_ = 0.0F;
    }
    return;
  }

  if (level_db > settings_.close_threshold_db) {
    quiet_time_ms_ = 0.0F;
    return;
  }

  quiet_time_ms_ += elapsed_ms;
  if (quiet_time_ms_ >= settings_.close_delay_ms) {
    open_ = false;
    quiet_time_ms_ = 0.0F;
  }
}

}
