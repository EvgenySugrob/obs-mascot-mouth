#include "mascot-source.hpp"

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mascot-mouth", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
  return "Lightweight microphone-reactive 2D mascot source";
}

MODULE_EXPORT const char *obs_module_name(void)
{
  return "Mascot Mouth";
}

bool obs_module_load(void)
{
  static obs_source_info source_info = mascot::MascotSource::source_info();
  obs_register_source(&source_info);
  blog(LOG_INFO, "[obs-mascot-mouth] Loaded version %s", MASCOT_PLUGIN_VERSION);
  return true;
}
