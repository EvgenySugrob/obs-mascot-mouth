#include "mascot-source.hpp"

#include <algorithm>
#include <cmath>
#include <new>

namespace {

constexpr const char *kAudioSource = "audio_source";
constexpr const char *kClosedImage = "closed_image";
constexpr const char *kMediumImage = "medium_image";
constexpr const char *kOpenImage = "open_image";
constexpr const char *kClosedBlinkImage = "closed_blink_image";
constexpr const char *kMediumBlinkImage = "medium_blink_image";
constexpr const char *kOpenBlinkImage = "open_blink_image";
constexpr const char *kOpenThreshold = "open_threshold_db";
constexpr const char *kWideThreshold = "wide_threshold_db";
constexpr const char *kCloseThreshold = "close_threshold_db";
constexpr const char *kCloseDelay = "close_delay_ms";
constexpr const char *kMouthSmoothing = "mouth_smoothing_ms";
constexpr const char *kBlinkEnabled = "blink_enabled";
constexpr const char *kBlinkMinimumInterval = "blink_minimum_interval_ms";
constexpr const char *kBlinkMaximumInterval = "blink_maximum_interval_ms";
constexpr const char *kBlinkDuration = "blink_duration_ms";
constexpr const char *kMotionEnabled = "motion_enabled";
constexpr const char *kMotionOffset = "motion_offset_pixels";
constexpr const char *kMotionScale = "motion_scale_percent";
constexpr const char *kMotionTilt = "motion_tilt_degrees";
constexpr const char *kIdleEnabled = "idle_enabled";
constexpr const char *kIdleAmount = "idle_amount_percent";
constexpr const char *kIdleSpeed = "idle_speed";

constexpr const char *kImageFilter = "PNG Files (*.png);;All Files (*.*)";

const char *source_name(void *)
{
  return obs_module_text("MascotSource");
}

}

namespace mascot {

MascotSource::MascotSource(obs_data_t *settings, obs_source_t *source) : source_(source)
{
  meter_ = obs_volmeter_create(OBS_FADER_LOG);
  if (meter_) {
    obs_volmeter_set_peak_meter_type(meter_, SAMPLE_PEAK_METER);
    obs_volmeter_add_callback(meter_, meter_updated, this);
  }
  update(settings);
}

MascotSource::~MascotSource()
{
  if (meter_) {
    obs_volmeter_remove_callback(meter_, meter_updated, this);
    obs_volmeter_detach_source(meter_);
    obs_volmeter_destroy(meter_);
    meter_ = nullptr;
  }

  obs_enter_graphics();
  {
    std::lock_guard<std::mutex> lock(image_mutex_);
    free_image_set(images_);
  }
  obs_leave_graphics();
}

void MascotSource::free_image(Image &image)
{
  if (image.initialized)
    gs_image_file_free(&image.file);
  image.file = {};
  image.initialized = false;
  image.path.clear();
}

void MascotSource::load_image(Image &image, const char *path)
{
  const std::string next_path = path ? path : "";
  if (next_path == image.path)
    return;

  obs_enter_graphics();
  {
    std::lock_guard<std::mutex> lock(image_mutex_);
    free_image(image);
    image.path = next_path;
    if (!image.path.empty()) {
      gs_image_file_init(&image.file, image.path.c_str());
      image.initialized = true;
      gs_image_file_init_texture(&image.file);
      if (!image.file.loaded) {
        blog(LOG_WARNING, "[obs-mascot-mouth] Could not load image: %s", image.path.c_str());
      }
    }
  }
  obs_leave_graphics();
}

void MascotSource::free_image_set(ImageSet &set)
{
  free_image(set.closed);
  free_image(set.medium);
  free_image(set.open);
  free_image(set.closed_blink);
  free_image(set.medium_blink);
  free_image(set.open_blink);
}

void MascotSource::attach_meter(const char *source_name_value)
{
  if (!meter_)
    return;

  obs_volmeter_detach_source(meter_);
  meter_channels_.store(0, std::memory_order_relaxed);
  latest_level_db_.store(-96.0F, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    mouth_.reset();
    animation_.reset();
    previous_mouth_level_ = MouthLevel::Closed;
    render_mouth_level_.store(static_cast<int>(MouthLevel::Closed), std::memory_order_relaxed);
    render_offset_y_.store(0.0F, std::memory_order_relaxed);
    render_scale_x_.store(1.0F, std::memory_order_relaxed);
    render_scale_y_.store(1.0F, std::memory_order_relaxed);
    render_rotation_.store(0.0F, std::memory_order_relaxed);
  }

  if (!source_name_value || !*source_name_value)
    return;

  obs_source_t *audio_source = obs_get_source_by_name(source_name_value);
  if (!audio_source) {
    blog(LOG_WARNING, "[obs-mascot-mouth] Audio source not found: %s", source_name_value);
    return;
  }

  const uint32_t flags = obs_source_get_output_flags(audio_source);
  if ((flags & OBS_SOURCE_AUDIO) == 0) {
    blog(LOG_WARNING, "[obs-mascot-mouth] Source has no audio output: %s", source_name_value);
    obs_source_release(audio_source);
    return;
  }

  if (obs_volmeter_attach_source(meter_, audio_source)) {
    meter_channels_.store(obs_volmeter_get_nr_channels(meter_), std::memory_order_relaxed);
  } else {
    blog(LOG_WARNING, "[obs-mascot-mouth] Could not attach to audio source: %s", source_name_value);
  }
  obs_source_release(audio_source);
}

void MascotSource::update(obs_data_t *settings)
{
  MouthSettings mouth_settings;
  mouth_settings.open_threshold_db = static_cast<float>(obs_data_get_double(settings, kOpenThreshold));
  mouth_settings.wide_threshold_db = static_cast<float>(obs_data_get_double(settings, kWideThreshold));
  mouth_settings.close_threshold_db = static_cast<float>(obs_data_get_double(settings, kCloseThreshold));
  mouth_settings.close_delay_ms = static_cast<float>(obs_data_get_int(settings, kCloseDelay));
  mouth_settings.smoothing_ms = static_cast<float>(obs_data_get_int(settings, kMouthSmoothing));
  BlinkSettings blink_settings;
  blink_settings.minimum_interval_ms =
      static_cast<float>(obs_data_get_int(settings, kBlinkMinimumInterval));
  blink_settings.maximum_interval_ms =
      static_cast<float>(obs_data_get_int(settings, kBlinkMaximumInterval));
  blink_settings.duration_ms = static_cast<float>(obs_data_get_int(settings, kBlinkDuration));
  AnimationSettings animation_settings;
  animation_settings.speech_enabled = obs_data_get_bool(settings, kMotionEnabled);
  animation_settings.bounce_pixels = static_cast<float>(obs_data_get_int(settings, kMotionOffset));
  animation_settings.stretch_percent =
      static_cast<float>(obs_data_get_double(settings, kMotionScale));
  animation_settings.tilt_degrees =
      static_cast<float>(obs_data_get_double(settings, kMotionTilt));
  animation_settings.idle_enabled = obs_data_get_bool(settings, kIdleEnabled);
  animation_settings.idle_amount_percent =
      static_cast<float>(obs_data_get_double(settings, kIdleAmount));
  animation_settings.idle_speed = static_cast<float>(obs_data_get_double(settings, kIdleSpeed));
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    mouth_.configure(mouth_settings);
    blink_.configure(blink_settings);
    animation_.configure(animation_settings);
    blinking_enabled_ = obs_data_get_bool(settings, kBlinkEnabled);
    render_blink_.store(false, std::memory_order_relaxed);
  }

  load_image(images_.closed, obs_data_get_string(settings, kClosedImage));
  load_image(images_.medium, obs_data_get_string(settings, kMediumImage));
  load_image(images_.open, obs_data_get_string(settings, kOpenImage));
  load_image(images_.closed_blink, obs_data_get_string(settings, kClosedBlinkImage));
  load_image(images_.medium_blink, obs_data_get_string(settings, kMediumBlinkImage));
  load_image(images_.open_blink, obs_data_get_string(settings, kOpenBlinkImage));
  attach_meter(obs_data_get_string(settings, kAudioSource));
}

void MascotSource::meter_updated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
                                 const float[MAX_AUDIO_CHANNELS],
                                 const float[MAX_AUDIO_CHANNELS])
{
  auto *self = static_cast<MascotSource *>(data);
  const int channel_count = std::clamp(self->meter_channels_.load(std::memory_order_relaxed), 1,
                                       static_cast<int>(MAX_AUDIO_CHANNELS));
  float level = -96.0F;
  for (int channel = 0; channel < channel_count; ++channel) {
    if (std::isfinite(magnitude[channel]))
      level = std::max(level, magnitude[channel]);
  }
  self->latest_level_db_.store(level, std::memory_order_relaxed);
}

void MascotSource::tick(float seconds)
{
  std::lock_guard<std::mutex> lock(state_mutex_);
  const float elapsed_ms = seconds * 1000.0F;
  mouth_.update(latest_level_db_.load(std::memory_order_relaxed), elapsed_ms);
  if (blinking_enabled_)
    blink_.update(elapsed_ms);
  const MouthLevel mouth_level = mouth_.level();
  const bool emphasis =
      mouth_level != previous_mouth_level_ && mouth_level != MouthLevel::Closed;
  animation_.update(mouth_.activity(), emphasis, elapsed_ms);
  const AnimationFrame &frame = animation_.frame();
  render_mouth_level_.store(static_cast<int>(mouth_level), std::memory_order_relaxed);
  render_blink_.store(blinking_enabled_ && blink_.is_blinking(), std::memory_order_relaxed);
  render_offset_y_.store(frame.offset_y, std::memory_order_relaxed);
  render_scale_x_.store(frame.scale_x, std::memory_order_relaxed);
  render_scale_y_.store(frame.scale_y, std::memory_order_relaxed);
  render_rotation_.store(frame.rotation_radians, std::memory_order_relaxed);
  previous_mouth_level_ = mouth_level;
}

void MascotSource::render(gs_effect_t *effect)
{
  std::lock_guard<std::mutex> lock(image_mutex_);
  const MouthLevel mouth_level =
      static_cast<MouthLevel>(render_mouth_level_.load(std::memory_order_relaxed));
  const bool is_blinking = render_blink_.load(std::memory_order_relaxed);
  Image &normal = normal_image(images_, mouth_level);
  Image &blink = blink_image(images_, mouth_level);
  Image *candidates[] = {is_blinking ? &blink : &normal, &normal, &images_.open, &images_.closed};
  gs_image_file *image = nullptr;
  for (Image *candidate : candidates) {
    gs_image_file *file = image_file(*candidate);
    if (candidate->initialized && file->texture) {
      image = file;
      break;
    }
  }
  if (!image)
    return;

  gs_eparam_t *parameter = gs_effect_get_param_by_name(effect, "image");
  gs_effect_set_texture(parameter, image->texture);
  const float scale_x = render_scale_x_.load(std::memory_order_relaxed);
  const float scale_y = render_scale_y_.load(std::memory_order_relaxed);
  const float rotation = render_rotation_.load(std::memory_order_relaxed);
  const float offset_y = render_offset_y_.load(std::memory_order_relaxed);
  const float center_x = static_cast<float>(image->cx) * 0.5F;
  const float center_y = static_cast<float>(image->cy) * 0.5F;
  gs_matrix_push();
  gs_matrix_translate3f(center_x, center_y + offset_y, 0.0F);
  gs_matrix_rotaa4f(0.0F, 0.0F, 1.0F, rotation);
  gs_matrix_scale3f(scale_x, scale_y, 1.0F);
  gs_matrix_translate3f(-center_x, -center_y, 0.0F);
  gs_draw_sprite(image->texture, 0, image->cx, image->cy);
  gs_matrix_pop();
}

uint32_t MascotSource::width() const
{
  std::lock_guard<std::mutex> lock(image_mutex_);
  return image_set_width(images_);
}

uint32_t MascotSource::height() const
{
  std::lock_guard<std::mutex> lock(image_mutex_);
  return image_set_height(images_);
}

MascotSource::Image &MascotSource::normal_image(ImageSet &set, MouthLevel level) noexcept
{
  if (level == MouthLevel::Medium)
    return set.medium;
  if (level == MouthLevel::Wide)
    return set.open;
  return set.closed;
}

MascotSource::Image &MascotSource::blink_image(ImageSet &set, MouthLevel level) noexcept
{
  if (level == MouthLevel::Medium)
    return set.medium_blink;
  if (level == MouthLevel::Wide)
    return set.open_blink;
  return set.closed_blink;
}

uint32_t MascotSource::image_set_width(const ImageSet &set) noexcept
{
  return std::max({image_file(set.closed)->cx, image_file(set.medium)->cx,
                   image_file(set.open)->cx, image_file(set.closed_blink)->cx,
                   image_file(set.medium_blink)->cx, image_file(set.open_blink)->cx});
}

uint32_t MascotSource::image_set_height(const ImageSet &set) noexcept
{
  return std::max({image_file(set.closed)->cy, image_file(set.medium)->cy,
                   image_file(set.open)->cy, image_file(set.closed_blink)->cy,
                   image_file(set.medium_blink)->cy, image_file(set.open_blink)->cy});
}

bool MascotSource::enum_audio_sources(void *data, obs_source_t *source)
{
  auto *property = static_cast<obs_property_t *>(data);
  if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0)
    obs_property_list_add_string(property, obs_source_get_name(source), obs_source_get_name(source));
  return true;
}

obs_properties_t *MascotSource::properties(void *)
{
  obs_properties_t *properties = obs_properties_create();
  obs_property_t *audio = obs_properties_add_list(properties, kAudioSource,
                                                  obs_module_text("AudioSource"), OBS_COMBO_TYPE_LIST,
                                                  OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(audio, obs_module_text("NoAudioSource"), "");
  obs_enum_sources(enum_audio_sources, audio);

  obs_properties_add_path(properties, kClosedImage, obs_module_text("ClosedImage"), OBS_PATH_FILE,
                          kImageFilter, nullptr);
  obs_properties_add_path(properties, kMediumImage, obs_module_text("MediumImage"), OBS_PATH_FILE,
                          kImageFilter, nullptr);
  obs_properties_add_path(properties, kOpenImage, obs_module_text("OpenImage"), OBS_PATH_FILE,
                          kImageFilter, nullptr);
  obs_properties_add_path(properties, kClosedBlinkImage, obs_module_text("ClosedBlinkImage"),
                          OBS_PATH_FILE, kImageFilter, nullptr);
  obs_properties_add_path(properties, kMediumBlinkImage, obs_module_text("MediumBlinkImage"),
                          OBS_PATH_FILE, kImageFilter, nullptr);
  obs_properties_add_path(properties, kOpenBlinkImage, obs_module_text("OpenBlinkImage"),
                          OBS_PATH_FILE, kImageFilter, nullptr);

  obs_properties_add_float_slider(properties, kOpenThreshold, obs_module_text("OpenThreshold"),
                                  -60.0, 0.0, 1.0);
  obs_properties_add_float_slider(properties, kWideThreshold, obs_module_text("WideThreshold"),
                                  -60.0, 0.0, 1.0);
  obs_properties_add_float_slider(properties, kCloseThreshold, obs_module_text("CloseThreshold"),
                                  -80.0, 0.0, 1.0);
  obs_properties_add_int_slider(properties, kCloseDelay, obs_module_text("CloseDelay"), 0, 500, 10);
  obs_properties_add_int_slider(properties, kMouthSmoothing, obs_module_text("MouthSmoothing"), 0,
                                500, 10);
  obs_properties_add_text(properties, "threshold_hint", obs_module_text("ThresholdHint"),
                          OBS_TEXT_INFO);
  obs_properties_add_bool(properties, kBlinkEnabled, obs_module_text("BlinkEnabled"));
  obs_properties_add_int_slider(properties, kBlinkMinimumInterval,
                                obs_module_text("BlinkMinimumInterval"), 250, 30000, 100);
  obs_properties_add_int_slider(properties, kBlinkMaximumInterval,
                                obs_module_text("BlinkMaximumInterval"), 250, 60000, 100);
  obs_properties_add_int_slider(properties, kBlinkDuration, obs_module_text("BlinkDuration"), 40,
                                1000, 10);
  obs_properties_add_bool(properties, kMotionEnabled, obs_module_text("MotionEnabled"));
  obs_properties_add_int_slider(properties, kMotionOffset, obs_module_text("MotionOffset"), 0, 20,
                                1);
  obs_properties_add_float_slider(properties, kMotionScale, obs_module_text("MotionScale"), 0.0,
                                  5.0, 0.1);
  obs_properties_add_float_slider(properties, kMotionTilt, obs_module_text("MotionTilt"), 0.0,
                                  10.0, 0.1);
  obs_properties_add_bool(properties, kIdleEnabled, obs_module_text("IdleEnabled"));
  obs_properties_add_float_slider(properties, kIdleAmount, obs_module_text("IdleAmount"), 0.0, 3.0,
                                  0.1);
  obs_properties_add_float_slider(properties, kIdleSpeed, obs_module_text("IdleSpeed"), 0.05, 1.0,
                                  0.05);
  obs_properties_add_text(properties, "animation_hint", obs_module_text("AnimationHint"),
                          OBS_TEXT_INFO);
  return properties;
}

void MascotSource::defaults(obs_data_t *settings)
{
  obs_data_set_default_string(settings, kAudioSource, "");
  obs_data_set_default_string(settings, kClosedImage, "");
  obs_data_set_default_string(settings, kMediumImage, "");
  obs_data_set_default_string(settings, kOpenImage, "");
  obs_data_set_default_string(settings, kClosedBlinkImage, "");
  obs_data_set_default_string(settings, kMediumBlinkImage, "");
  obs_data_set_default_string(settings, kOpenBlinkImage, "");
  obs_data_set_default_double(settings, kOpenThreshold, -35.0);
  obs_data_set_default_double(settings, kWideThreshold, -22.0);
  obs_data_set_default_double(settings, kCloseThreshold, -42.0);
  obs_data_set_default_int(settings, kCloseDelay, 120);
  obs_data_set_default_int(settings, kMouthSmoothing, 80);
  obs_data_set_default_bool(settings, kBlinkEnabled, true);
  obs_data_set_default_int(settings, kBlinkMinimumInterval, 3500);
  obs_data_set_default_int(settings, kBlinkMaximumInterval, 6500);
  obs_data_set_default_int(settings, kBlinkDuration, 120);
  obs_data_set_default_bool(settings, kMotionEnabled, true);
  obs_data_set_default_int(settings, kMotionOffset, 4);
  obs_data_set_default_double(settings, kMotionScale, 1.5);
  obs_data_set_default_double(settings, kMotionTilt, 1.5);
  obs_data_set_default_bool(settings, kIdleEnabled, true);
  obs_data_set_default_double(settings, kIdleAmount, 0.6);
  obs_data_set_default_double(settings, kIdleSpeed, 0.25);
}

obs_source_info MascotSource::source_info()
{
  obs_source_info info{};
  info.id = "mascot_mouth_source";
  info.type = OBS_SOURCE_TYPE_INPUT;
  info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
  info.get_name = source_name;
  info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
    try {
      return new MascotSource(settings, source);
    } catch (const std::bad_alloc &) {
      blog(LOG_ERROR, "[obs-mascot-mouth] Out of memory while creating source");
      return nullptr;
    }
  };
  info.destroy = [](void *data) { delete static_cast<MascotSource *>(data); };
  info.update = [](void *data, obs_data_t *settings) {
    static_cast<MascotSource *>(data)->update(settings);
  };
  info.get_defaults = defaults;
  info.get_properties = properties;
  info.video_tick = [](void *data, float seconds) { static_cast<MascotSource *>(data)->tick(seconds); };
  info.video_render = [](void *data, gs_effect_t *effect) {
    static_cast<MascotSource *>(data)->render(effect);
  };
  info.get_width = [](void *data) { return static_cast<MascotSource *>(data)->width(); };
  info.get_height = [](void *data) { return static_cast<MascotSource *>(data)->height(); };
  return info;
}

}
