/** @file RedLilyGOCapability.h @brief Shared runtime capability settings API. */

#pragma once

#include <Preferences.h>

#define CATEGORY_USB 1
#define CATEGORY_WIFI 2
#define CATEGORY_TFT 3

class RedLilyGOCapability
{

public:
  /** @brief Construct capability base. */
  RedLilyGOCapability(void) {}

  /** @brief Initialize capability from persistent preferences. */
  virtual void begin(Preferences &prefs) = 0;
  /** @brief Service capability state from the Arduino loop. */
  virtual void loop(Preferences &prefs) = 0;
  /** @brief Stop capability; default has no work. */
  virtual void end()
  {
  }

  enum class SettingType : uint8_t
  {
    String = 0, ///< NUL-terminated text preference.
    Bool = 1,   ///< Boolean preference.
    Int16 = 2,  ///< Signed 16-bit preference.
    UInt16 = 3  ///< Unsigned 16-bit preference.
  };

protected:
  /** @brief Register string setting with default value. */
  void registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const char* defaultValue);
  /** @brief Register signed 16-bit setting with default value. */
  void registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const int16_t defaultValue);
  /** @brief Register unsigned 16-bit setting with default value. */
  void registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const uint16_t defaultValue);
  /** @brief Register boolean setting with default value. */
  void registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const bool defaultValue);
};
