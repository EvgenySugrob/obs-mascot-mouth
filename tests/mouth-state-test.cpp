#include "blink-state.hpp"
#include "mouth-state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using mascot::BlinkSettings;
using mascot::BlinkState;
using mascot::MouthSettings;
using mascot::MouthState;

static void opens_at_threshold()
{
  MouthState mouth;
  mouth.update(-35.0F, 16.0F);
  assert(mouth.is_open());
}

static void hysteresis_prevents_chatter()
{
  MouthState mouth;
  mouth.update(-20.0F, 16.0F);
  mouth.update(-39.0F, 500.0F);
  assert(mouth.is_open());
}

static void closes_only_after_delay()
{
  MouthState mouth(MouthSettings{-35.0F, -42.0F, 120.0F});
  mouth.update(-20.0F, 16.0F);
  mouth.update(-60.0F, 100.0F);
  assert(mouth.is_open());
  mouth.update(-60.0F, 20.0F);
  assert(!mouth.is_open());
}

static void speech_resets_close_timer()
{
  MouthState mouth(MouthSettings{-35.0F, -42.0F, 120.0F});
  mouth.update(-20.0F, 16.0F);
  mouth.update(-60.0F, 100.0F);
  mouth.update(-30.0F, 16.0F);
  mouth.update(-60.0F, 100.0F);
  assert(mouth.is_open());
}

static void sanitizes_settings_and_bad_samples()
{
  MouthState mouth(MouthSettings{-80.0F, -10.0F, -50.0F});
  assert(mouth.settings().close_threshold_db == -80.0F);
  assert(mouth.settings().close_delay_ms == 0.0F);
  mouth.update(std::nanf(""), 20.0F);
  assert(!mouth.is_open());
}

static void blink_uses_interval_and_duration()
{
  BlinkState blink(BlinkSettings{1000.0F, 1000.0F, 100.0F});
  blink.update(999.0F);
  assert(!blink.is_blinking());
  blink.update(1.0F);
  assert(blink.is_blinking());
  blink.update(99.0F);
  assert(blink.is_blinking());
  blink.update(1.0F);
  assert(!blink.is_blinking());
}

static void blink_sanitizes_settings()
{
  BlinkState blink(BlinkSettings{-10.0F, 20.0F, 0.0F});
  assert(blink.settings().minimum_interval_ms == 250.0F);
  assert(blink.settings().maximum_interval_ms == 250.0F);
  assert(blink.settings().duration_ms == 40.0F);
  blink.update(std::nanf(""));
  assert(!blink.is_blinking());
}

int main()
{
  opens_at_threshold();
  hysteresis_prevents_chatter();
  closes_only_after_delay();
  speech_resets_close_timer();
  sanitizes_settings_and_bad_samples();
  blink_uses_interval_and_duration();
  blink_sanitizes_settings();
  std::cout << "All core tests passed\n";
  return 0;
}
