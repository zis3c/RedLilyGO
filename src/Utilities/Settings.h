/** @file Settings.h @brief Persistent setting registration helpers. */

#pragma once

#include <cstdint>
#include <string>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "../RedLilyGOCapability.h"

/** @brief Register one persistent setting metadata entry. */
void registerSettingName(const uint8_t group, const std::string &name, const RedLilyGOCapability::SettingType &type, const std::string& defaultValue);
/** @brief Serialize registered settings and current values into JSON. */
void enumerateSettingsAsJson(Preferences &prefs, JsonArray array);
/** @brief Validate and persist one setting value; return false on failure. */
bool setSettingValue(Preferences &prefs, const std::string &name, const std::string &value);
/** @brief Read integer setting and report parse error through error. */
uint16_t getIntegerSettingValue(Preferences &prefs, const std::string &name, bool &error);
/** @brief Restore all registered settings to defaults. */
void resetSettings(Preferences &prefs);
