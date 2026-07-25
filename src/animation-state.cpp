#include "animation-state.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846F;

float finite_or(float value, float fallback) noexcept
{
  return std::isfinite(value) ? value : fallback;
}

}

namespace mascot {

AnimationState::AnimationState() noexcept : AnimationState(AnimationSettings{}) {}

AnimationState::AnimationState(AnimationSettings settings) noexcept
{
  configure(settings);
}

void AnimationState::configure(AnimationSettings settings) noexcept
{
  settings.bounce_pixels = std::clamp(finite_or(settings.bounce_pixels, 4.0F), 0.0F, 30.0F);
  settings.stretch_percent =
      std::clamp(finite_or(settings.stretch_percent, 1.5F), 0.0F, 10.0F);
  settings.tilt_degrees = std::clamp(finite_or(settings.tilt_degrees, 1.5F), 0.0F, 10.0F);
  settings.idle_amount_percent =
      std::clamp(finite_or(settings.idle_amount_percent, 0.6F), 0.0F, 5.0F);
  settings.idle_speed = std::clamp(finite_or(settings.idle_speed, 0.25F), 0.05F, 2.0F);
  settings_ = settings;
}

void AnimationState::reset() noexcept
{
  frame_ = {};
  speech_activity_ = 0.0F;
  speech_impulse_ = 0.0F;
  speech_phase_ = 0.0F;
  idle_phase_ = 0.0F;
}

void AnimationState::update(float activity, bool emphasis, float elapsed_ms) noexcept
{
  const float elapsed_seconds =
      std::clamp(finite_or(elapsed_ms, 0.0F), 0.0F, 100.0F) * 0.001F;
  const float normalized_activity = std::clamp(finite_or(activity, 0.0F), 0.0F, 1.0F);
  if (emphasis && settings_.speech_enabled)
    speech_impulse_ = 1.0F;
  speech_impulse_ *= std::exp(-elapsed_seconds * 9.0F);
  const float target_activity =
      settings_.speech_enabled
          ? std::clamp(normalized_activity * 0.65F + speech_impulse_ * 0.55F, 0.0F, 1.0F)
          : 0.0F;
  const float response = 1.0F - std::exp(-elapsed_seconds * 14.0F);
  speech_activity_ += (target_activity - speech_activity_) * response;
  speech_phase_ += elapsed_seconds * (8.0F + speech_activity_ * 8.0F);
  idle_phase_ += elapsed_seconds * settings_.idle_speed * 2.0F * kPi;

  if (speech_activity_ > 0.01F) {
    const float pulse = 0.6F + 0.4F * std::abs(std::sin(speech_phase_));
    const float stretch = settings_.stretch_percent * 0.01F * speech_activity_ * pulse;
    frame_.offset_y = -settings_.bounce_pixels * speech_activity_ * pulse;
    frame_.scale_x = std::max(0.8F, 1.0F - stretch * 0.4F);
    frame_.scale_y = 1.0F + stretch;
    frame_.rotation_radians =
        settings_.tilt_degrees * kPi / 180.0F * speech_activity_ *
        std::sin(speech_phase_ * 0.72F);
    return;
  }

  frame_.offset_y = 0.0F;
  frame_.rotation_radians = 0.0F;
  if (settings_.idle_enabled) {
    const float breath = 0.5F + 0.5F * std::sin(idle_phase_);
    const float scale = settings_.idle_amount_percent * 0.01F * breath;
    frame_.scale_x = 1.0F - scale * 0.25F;
    frame_.scale_y = 1.0F + scale;
  } else {
    frame_.scale_x = 1.0F;
    frame_.scale_y = 1.0F;
  }
}

const AnimationSettings &AnimationState::settings() const noexcept
{
  return settings_;
}

const AnimationFrame &AnimationState::frame() const noexcept
{
  return frame_;
}

}
