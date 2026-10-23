/** @file settings.h @brief Marauder settings persistence API. */

#pragma once

#ifndef Settings_h
#define Settings_h

#include "configs.h"

#include "SPIFFS.h"
#include <FS.h>
#include <ArduinoJson.h>

#define FORMAT_SPIFFS_IF_FAILED true

#ifdef HAS_SCREEN
  #include "Display.h"

  extern Display display_obj;
#endif

class Settings {

  private:
    String json_settings_string;

  public:
    /** @brief Mount default SPIFFS settings file and load JSON. */
    bool begin();
    /** @brief Load JSON settings from filesystem path. */
    bool begin(fs::FS fs, String filename);

    /** @brief Read typed setting value, returning type default when absent. */
    template <typename T>
    T loadSetting(String name);

    /** @brief Persist boolean setting and refresh cached JSON. */
    template <typename T>
    T saveSetting(String key, bool value);

    /** @brief Toggle boolean setting and return resulting value. */
    bool toggleSetting(String key);
    /** @brief Return stored setting type label. */
    String getSettingType(String key);
    /** @brief Return setting name at zero-based JSON index. */
    String setting_index_to_name(int i);
    /** @brief Return number of entries in loaded settings JSON. */
    int getNumberSettings();

    //template<>
    //int loadSetting<int>(String key);
    
    //template<>
    //String loadSetting<String>(String key);
    
    //template<>
    //bool loadSetting<bool>(String key);
    
    //template<>
    //uint8_t loadSetting<uint8_t>(String key);

    /** @brief Return cached serialized settings JSON. */
    String getSettingsString();
    /** @brief Create default settings at standard path. */
    bool createDefaultSettings(fs::FS &fs);
    /** @brief Create default settings at supplied path. */
    bool createDefaultSettings(fs::FS &fs, String filename);
    /** @brief Print settings JSON in human-readable form. */
    void printJsonSettings(String json_string);
    /** @brief Service periodic settings work; currently no periodic action. */
    void main(uint32_t currentTime);
};

#endif
