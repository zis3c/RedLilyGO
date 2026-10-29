/** @file RedLilyGOCapability.cpp @brief Shared capability and setting registry implementation. */

#include "RedLilyGOCapability.h"
#include "Utilities/Settings.h"

#include <string>

void RedLilyGOCapability::registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const char *defaultValue)
{
    registerSettingName(group, name, type, defaultValue);
}

void RedLilyGOCapability::registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const int16_t defaultValue)
{
    registerSettingName(group, name, type, std::to_string(defaultValue));
}

void RedLilyGOCapability::registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const uint16_t defaultValue)
{
    registerSettingName(group, name, type, std::to_string(defaultValue));
}

void RedLilyGOCapability::registerUserConfigurableSetting(const uint8_t group, const char *name, const RedLilyGOCapability::SettingType type, const bool defaultValue)
{
    registerSettingName(group, name, type, std::to_string(defaultValue));
}
