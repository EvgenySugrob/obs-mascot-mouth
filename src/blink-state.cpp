#include "blink-state.hpp"

#include <algorithm>
#include <cmath>

namespace mascot {

BlinkState::BlinkState(BlinkSettings settings, std::uint32_t seed)
    : settings_(sanitize(settings)), random_state_(seed == 0 ? 0x9E3779B9U : seed)
{
  reset();
}

BlinkSettings BlinkState::sanitize(BlinkSettings settings) noexcept
{
  if (!std::isfinite(settings.minimum_interval_ms))
    settings.minimum_interval_ms = 3500.0F;
  if (!std::isfinite(settings.maximum_interval_ms))
    settings.maximum_interval_ms = 6500.0F;
  if (!std::isfinite(settings.duration_ms))
    settings.duration_ms = 120.0F;

  settings.minimum_interval_ms = std::clamp(settings.minimum_interval_ms, 250.0F, 60000.0F);
  settings.maximum_interval_ms = std::clamp(settings.maximum_interval_ms, 250.0F, 60000.0F);
  if (settings.maximum_interval_ms < settings.minimum_interval_ms)
    settings.maximum_interval_ms = settings.minimum_interval_ms;
  settings.duration_ms = std::clamp(settings.duration_ms, 40.0F, 2000.0F);
  return settings;
}

void BlinkState::configure(BlinkSettings settings)
{
  settings_ = sanitize(settings);
  reset();
}

void BlinkState::reset() noexcept
{
  blinking_ = false;
  phase_remaining_ms_ = next_interval();
}

float BlinkState::next_interval() noexcept
{
  random_state_ = random_state_ * 1664525U + 1013904223U;
  const float unit = static_cast<float>(random_state_ >> 8U) / 16777215.0F;
  return settings_.minimum_interval_ms +
         (settings_.maximum_interval_ms - settings_.minimum_interval_ms) * unit;
}

void BlinkState::update(float elapsed_ms) noexcept
{
  if (!std::isfinite(elapsed_ms) || elapsed_ms <= 0.0F)
    return;

  elapsed_ms = std::min(elapsed_ms, 10000.0F);
  while (elapsed_ms >= phase_remaining_ms_) {
    elapsed_ms -= phase_remaining_ms_;
    blinking_ = !blinking_;
    phase_remaining_ms_ = blinking_ ? settings_.duration_ms : next_interval();
  }
  phase_remaining_ms_ -= elapsed_ms;
}

}
