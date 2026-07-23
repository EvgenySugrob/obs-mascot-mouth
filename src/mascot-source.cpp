#include "mascot-source.hpp"

#include <algorithm>
#include <cmath>
#include <new>

namespace {

constexpr const char *kAudioSource = "audio_source";
constexpr const char *kClosedImage = "closed_image";
constexpr const char *kOpenImage = "open_image";
constexpr const char *kOpenThreshold = "open_threshold_db";
constexpr const char *kCloseThreshold = "close_threshold_db";
constexpr const char *kCloseDelay = "close_delay_ms";

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
    free_image(closed_);
    free_image(open_);
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
    render_open_.store(false, std::memory_order_relaxed);
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
  mouth_settings.close_threshold_db = static_cast<float>(obs_data_get_double(settings, kCloseThreshold));
  mouth_settings.close_delay_ms = static_cast<float>(obs_data_get_int(settings, kCloseDelay));
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    mouth_.configure(mouth_settings);
  }

  load_image(closed_, obs_data_get_string(settings, kClosedImage));
  load_image(open_, obs_data_get_string(settings, kOpenImage));
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
  mouth_.update(latest_level_db_.load(std::memory_order_relaxed), seconds * 1000.0F);
  render_open_.store(mouth_.is_open(), std::memory_order_relaxed);
}

void MascotSource::render(gs_effect_t *effect)
{
  std::lock_guard<std::mutex> lock(image_mutex_);
  const bool is_open = render_open_.load(std::memory_order_relaxed);
  Image &selected = is_open ? open_ : closed_;
  Image &fallback = is_open ? closed_ : open_;
  gs_image_file *image = image_file(selected);
  if (!selected.initialized || !image->texture)
    image = image_file(fallback);
  if (!image->texture)
    return;

  gs_eparam_t *parameter = gs_effect_get_param_by_name(effect, "image");
  gs_effect_set_texture(parameter, image->texture);
  gs_draw_sprite(image->texture, 0, image->cx, image->cy);
}

uint32_t MascotSource::width() const
{
  std::lock_guard<std::mutex> lock(image_mutex_);
  return std::max(image_file(closed_)->cx, image_file(open_)->cx);
}

uint32_t MascotSource::height() const
{
  std::lock_guard<std::mutex> lock(image_mutex_);
  return std::max(image_file(closed_)->cy, image_file(open_)->cy);
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
  obs_properties_add_path(properties, kOpenImage, obs_module_text("OpenImage"), OBS_PATH_FILE,
                          kImageFilter, nullptr);

  obs_properties_add_float_slider(properties, kOpenThreshold, obs_module_text("OpenThreshold"),
                                  -60.0, 0.0, 1.0);
  obs_properties_add_float_slider(properties, kCloseThreshold, obs_module_text("CloseThreshold"),
                                  -80.0, 0.0, 1.0);
  obs_properties_add_int_slider(properties, kCloseDelay, obs_module_text("CloseDelay"), 0, 500, 10);
  obs_properties_add_text(properties, "threshold_hint", obs_module_text("ThresholdHint"),
                          OBS_TEXT_INFO);
  return properties;
}

void MascotSource::defaults(obs_data_t *settings)
{
  obs_data_set_default_string(settings, kAudioSource, "");
  obs_data_set_default_string(settings, kClosedImage, "");
  obs_data_set_default_string(settings, kOpenImage, "");
  obs_data_set_default_double(settings, kOpenThreshold, -35.0);
  obs_data_set_default_double(settings, kCloseThreshold, -42.0);
  obs_data_set_default_int(settings, kCloseDelay, 120);
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
