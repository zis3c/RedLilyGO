/** @file settings.cpp @brief ESP32 Marauder runtime configuration storage. */

#include "settings.h"

#define DEFAULT_SETTING_FILE "/settings.json"

/** @brief Return cached serialized settings JSON. */
String Settings::getSettingsString() {
  return this->json_settings_string;
}

/** @brief Load settings JSON from a selected filesystem path. */
bool Settings::begin(fs::FS fs, String filename) {
  if (!fs.exists(filename))
  {
    esp32m_println("Could not find settings file");
    if (!this->createDefaultSettings(fs, filename))
    {
      return false;
    }
  }

  File settingsFile = fs.open(filename, FILE_READ);
  if (!settingsFile)
  {
    esp32m_println("Settings file could not be opened");
    return false;
  }

  String json_string;
  DynamicJsonDocument jsonBuffer(1024);
  DeserializationError error = deserializeJson(jsonBuffer, settingsFile);
  serializeJson(jsonBuffer, json_string);
  this->json_settings_string = json_string;

  return true;
}

/** @brief Mount SPIFFS without formatting and load default settings file. */
bool Settings::begin() {
  // This code path is used when RedLilyGO is running without an SD card
  // In this mode we don't want to ever erase the filesystem as this has the users config file on it
  // As such if SPIFFS is broken its up to them to push a filesystem image to it
  if(!SPIFFS.begin(false)){
    esp32m_println("Settings SPIFFS Mount Failed");
    return false;
  }

  File settingsFile;

  if (SPIFFS.exists(DEFAULT_SETTING_FILE)) {
    settingsFile = SPIFFS.open(DEFAULT_SETTING_FILE, FILE_READ);
    
    if (!settingsFile) {
      settingsFile.close();
      esp32m_println(F("Could not find settings file"));
      if (this->createDefaultSettings(SPIFFS))
        return true;
      else
        return false;    
    }
  }
  else {
    esp32m_println("Settings file does not exist");
    if (this->createDefaultSettings(SPIFFS))
      return true;
    else
      return false;
  }

  String json_string;
  DynamicJsonDocument jsonBuffer(1024);
  DeserializationError error = deserializeJson(jsonBuffer, settingsFile);
  serializeJson(jsonBuffer, json_string);

  this->json_settings_string = json_string;
  
  return true;
}

/** @brief Generic unsupported setting type fallback. */
template <typename T>
T Settings::loadSetting(String key) {}

// Get type int settings
template<>
int Settings::loadSetting<int>(String key) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  return 0;
}

// Get type string settings
template<>
String Settings::loadSetting<String>(String key) {
  //return this->json_settings_string;
  
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  return "";
}

// Get type bool settings
template<>
bool Settings::loadSetting<bool>(String key) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  return false;
}

//Get type uint8_t settings
template<>
uint8_t Settings::loadSetting<uint8_t>(String key) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  return 0;
}

/** @brief Generic boolean-save template fallback. */
template <typename T>
T Settings::saveSetting(String key, bool value) {}

template<>
bool Settings::saveSetting<bool>(String key, bool value) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  String settings_string;

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key) {
      json["Settings"][i]["value"] = value;

      esp32m_println("Saving setting...");

      File settingsFile = SPIFFS.open("/settings.json", FILE_WRITE);

      if (!settingsFile) {
        esp32m_println(F("Failed to create settings file"));
        return false;
      }

      if (serializeJson(json, settingsFile) == 0) {
        esp32m_println(F("Failed to write to file"));
      }
      if (serializeJson(json, settings_string) == 0) {
        esp32m_println(F("Failed to write to string"));
      }
    
      // Close the file
      settingsFile.close();
    
      this->json_settings_string = settings_string;
    
      this->printJsonSettings(settings_string);
      
      return true;
    }
  }
  return false;
}

/** @brief Toggle named boolean setting and persist result. */
bool Settings::toggleSetting(String key) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
    return false;
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key) {
      if (json["Settings"][i]["value"]) {
        saveSetting<bool>(key, false);
        esp32m_println("Setting value to false");
        return false;
      }
      else {
        saveSetting<bool>(key, true);
        esp32m_println("Setting value to true");
        return true;
      }

      return false;
    }
  }

  return true;
}

/** @brief Return setting name at zero-based index. */
String Settings::setting_index_to_name(int i) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  return json["Settings"][i]["name"];
}

/** @brief Return number of loaded settings entries. */
int Settings::getNumberSettings() {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
  }

  return json["Settings"].size();
}

/** @brief Return type label for named setting. */
String Settings::getSettingType(String key) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    esp32m_println("\nCould not parse json");
    return "";
  }
  
  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["type"];
  }

  return "";
}

/** @brief Print parsed settings entries for diagnostics. */
void Settings::printJsonSettings(String json_string) {
  DynamicJsonDocument json(1024); // ArduinoJson v6

  if (deserializeJson(json, json_string)) {
    esp32m_println("\nCould not parse json");
  }
  
  esp32m_println("Settings\n----------------------------------------------");
  for (int i = 0; i < json["Settings"].size(); i++) {
    esp32m_println("Name: " + json["Settings"][i]["name"].as<String>());
    esp32m_println("Type: " + json["Settings"][i]["type"].as<String>());
    esp32m_println("Value: " + json["Settings"][i]["value"].as<String>());
    esp32m_println("----------------------------------------------");
  }
}

/** @brief Create defaults at standard settings path. */
bool Settings::createDefaultSettings(fs::FS &fs) {
  return createDefaultSettings(fs, DEFAULT_SETTING_FILE);
}

/** @brief Create and cache default JSON settings at filename. */
bool Settings::createDefaultSettings(fs::FS &fs, String filename) {
  esp32m_println(F("Creating default settings file: settings.json"));
  
  File settingsFile = fs.open(filename, FILE_WRITE);

  if (!settingsFile) {
    esp32m_println(F("Failed to create settings file"));
    return false;
  }

  DynamicJsonDocument jsonBuffer(1024);
  String settings_string;

  //jsonBuffer["Settings"][0]["name"] = "Channel";
  //jsonBuffer["Settings"][0]["type"] = "uint8_t";
  //jsonBuffer["Settings"][0]["value"] = 11;
  //jsonBuffer["Settings"][0]["range"]["min"] = 1;
  //jsonBuffer["Settings"][0]["range"]["max"] = 14;

  //jsonBuffer["Settings"][1]["name"] = "Channel Hop Delay";
  //jsonBuffer["Settings"][1]["type"] = "int";
  //jsonBuffer["Settings"][1]["value"] = 1;
  //jsonBuffer["Settings"][1]["range"]["min"] = 1;
  //jsonBuffer["Settings"][1]["range"]["max"] = 10;

  jsonBuffer["Settings"][0]["name"] = "ForcePMKID";
  jsonBuffer["Settings"][0]["type"] = "bool";
  jsonBuffer["Settings"][0]["value"] = true;
  jsonBuffer["Settings"][0]["range"]["min"] = false;
  jsonBuffer["Settings"][0]["range"]["max"] = true;

  jsonBuffer["Settings"][1]["name"] = "ForceProbe";
  jsonBuffer["Settings"][1]["type"] = "bool";
  jsonBuffer["Settings"][1]["value"] = true;
  jsonBuffer["Settings"][1]["range"]["min"] = false;
  jsonBuffer["Settings"][1]["range"]["max"] = true;

  jsonBuffer["Settings"][2]["name"] = "SavePCAP";
  jsonBuffer["Settings"][2]["type"] = "bool";
  jsonBuffer["Settings"][2]["value"] = true;
  jsonBuffer["Settings"][2]["range"]["min"] = false;
  jsonBuffer["Settings"][2]["range"]["max"] = true;

  jsonBuffer["Settings"][3]["name"] = "EnableLED";
  jsonBuffer["Settings"][3]["type"] = "bool";
  jsonBuffer["Settings"][3]["value"] = true;
  jsonBuffer["Settings"][3]["range"]["min"] = false;
  jsonBuffer["Settings"][3]["range"]["max"] = true;

  //jsonBuffer.printTo(settingsFile);
  if (serializeJson(jsonBuffer, settingsFile) == 0) {
    esp32m_println(F("Failed to write to file"));
  }
  if (serializeJson(jsonBuffer, settings_string) == 0) {
    esp32m_println(F("Failed to write to string"));
  }

  // Close the file
  settingsFile.close();

  this->json_settings_string = settings_string;

  this->printJsonSettings(settings_string);

  return true;
}

/** @brief Service periodic settings work; intentionally empty. */
void Settings::main(uint32_t currentTime) {
  
}
