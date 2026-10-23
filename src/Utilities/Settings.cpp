/** @file Settings.cpp @brief Persistent firmware settings accessors. */

#include "Settings.h"
#include "../RedLilyGOCapability.h"
#include "Diagnostics/Logging.h"

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <ArduinoJson.h>

#define LOG_SETTINGS "Settings"

static std::unordered_map<uint8_t, std::vector<std::tuple<std::string, RedLilyGOCapability::SettingType, std::string>>> settingLookup;

static bool isSensitiveSetting(const std::string &name)
{
    return name == "wifi-pwd" || name == "web-pass";
}

// NVS keys are limited to 15 characters (NVS_KEY_NAME_MAX_SIZE - 1); longer
// names silently fail to persist via Preferences on ESP32.
#define NVS_MAX_KEY_LEN 15

void registerSettingName(const uint8_t group, const std::string &name, const RedLilyGOCapability::SettingType &type, const std::string &defaultValue)
{
    if (name.length() > NVS_MAX_KEY_LEN)
    {
        Debug::Log.error(LOG_SETTINGS, "Setting name '" + name + "' exceeds NVS key limit of " + std::to_string(NVS_MAX_KEY_LEN) + " chars and will not persist");
    }

    if (settingLookup.find(group) == settingLookup.cend())
    {
        settingLookup[group] = std::vector<std::tuple<std::string, RedLilyGOCapability::SettingType, std::string>>();
    }

    // Keep each setting name unique within its category.
    const bool notFound = std::none_of(settingLookup[group].cbegin(), settingLookup[group].cend(),
                                       [&name](const std::tuple<std::string, RedLilyGOCapability::SettingType, std::string> &item)
                                       {
                                           return std::get<0>(item) == name;
                                       });

    if (notFound)
    {
        settingLookup[group].emplace_back(name, type, defaultValue);
    }
}

bool setSettingValue(Preferences &prefs, const std::string &name, const std::string &value)
{
    bool ret = false;

    try
    {
    int tempInt = 0;

    for (const auto &settingCategory : settingLookup)
    {
        for (const auto &setting : settingCategory.second)
        {
            if (name == get<0>(setting))
            {
                const RedLilyGOCapability::SettingType &settingType = get<1>(setting);

                switch (settingType)
                {
                case RedLilyGOCapability::SettingType::String:
                    prefs.putString(name.c_str(), value.c_str());
                    ret = prefs.getString(name.c_str(), "") == value.c_str();
                    break;
                case RedLilyGOCapability::SettingType::Bool:
                    if (value == "0" || value == "1")
                    {
                        ret = prefs.putBool(name.c_str(), value == "1") > 0;
                    }
                    else
                    {
                        std::string valueUpper = value;
                        std::transform(valueUpper.begin(), valueUpper.end(), valueUpper.begin(),
                                       [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
                        if (valueUpper == "TRUE")
                        {
                            ret = prefs.putBool(name.c_str(), true) > 0;
                        }
                        else if (valueUpper == "FALSE")
                        {
                            ret = prefs.putBool(name.c_str(), false) > 0;
                        }
                        else
                        {
                            Debug::Log.info(LOG_SETTINGS, "Invalid setting value, bool is too large");
                        }
                    }          
                    break;
                case RedLilyGOCapability::SettingType::Int16:
                    {
                    std::size_t parsed = 0;
                    tempInt = std::stoi(value, &parsed, 10);
                    if (parsed == value.size() && tempInt <= static_cast<int>(INT16_MAX) && tempInt >= static_cast<int>(INT16_MIN))
                    {
                        int16_t int16Value = 0;
                        int16Value = static_cast<int16_t>(tempInt);
                        ret = prefs.putShort(name.c_str(), int16Value) > 0;
                    }
                    else
                    {
                        Debug::Log.info(LOG_SETTINGS, "Invalid setting value");
                    }
                    }
                    break;
                case RedLilyGOCapability::SettingType::UInt16:
                    if (value.rfind("0x", 0) == 0)
                    {
                        std::size_t parsed = 0;
                        tempInt = std::stoi(value, &parsed, 16);
                        if (parsed == value.size() && tempInt <= static_cast<int>(UINT16_MAX) && tempInt >= 0)
                        {
                            uint16_t uint16Value = 0;
                            uint16Value = static_cast<uint16_t>(tempInt);
                            ret = prefs.putUShort(name.c_str(), uint16Value) > 0;
                        }
                        else
                        {
                            Debug::Log.info(LOG_SETTINGS, "Invalid setting value, invalid value");
                        }
                    }
                    else
                    {
                        Debug::Log.info(LOG_SETTINGS, "Invalid setting value, must start with 0x");
                    }
                    break;
                default:
                    Debug::Log.info(LOG_SETTINGS, "Unknown setting type");
                    break;
                };
            }
        }
    }
    }
    catch (const std::exception &error)
    {
        Debug::Log.info(LOG_SETTINGS, std::string("Invalid numeric setting: ") + error.what());
        return false;
    }

    return ret;
}

uint16_t getIntegerSettingValue(Preferences &prefs, const std::string &name, bool &error)
{
    error = false;

    for (const auto &settingCategory : settingLookup)
    {
        const std::vector<std::tuple<std::string, RedLilyGOCapability::SettingType, std::string>> &settingInCategory = settingCategory.second;
        for (const auto &setting : settingInCategory)
        {
            const std::string &settingName = get<0>(setting);
            const RedLilyGOCapability::SettingType &settingType = get<1>(setting);

            if (settingName != name)
            {
                continue;
            }
            else if (settingType == RedLilyGOCapability::SettingType::String)
            {
                goto error;
            }
            else if (!prefs.isKey(settingName.c_str()))
            {
                const std::string &defaultValue = get<2>(setting);
                // Stay consistent with stored values above: a negative Int16
                // is rejected rather than silently wrapped by atoi() into a
                // huge unsigned number (e.g. -5 -> 65531).
                if (settingType == RedLilyGOCapability::SettingType::Int16 &&
                    defaultValue.rfind('-', 0) == 0)
                {
                    error = true;
                    return 0;
                }
                return (uint16_t)atoi(defaultValue.c_str());
            }
            else
            {
                switch (settingType)
                {
                case RedLilyGOCapability::SettingType::Bool:
                    return prefs.getBool(settingName.c_str());
                    break;
                case RedLilyGOCapability::SettingType::Int16:
                {
                    const int16_t value = prefs.getShort(settingName.c_str());
                    if (value < 0)
                    {
                        error = true;
                        return 0;
                    }
                    return static_cast<uint16_t>(value);
                }
                    break;
                case RedLilyGOCapability::SettingType::UInt16:
                    return prefs.getUShort(settingName.c_str());
                default: // string and other types
                    break;
                };
            }
        }
    }

error:
    error = true;
    return 0;
}

void enumerateSettingsAsJson(Preferences &prefs, JsonArray array)
{
    for (const auto &settingCategory : settingLookup)
    {
        uint8_t category = settingCategory.first;
        const std::vector<std::tuple<std::string, RedLilyGOCapability::SettingType, std::string>> &settingInCategory = settingCategory.second;

        const JsonObject categoryObj = array.add<JsonObject>();

        switch (category)
        {
        case CATEGORY_USB:
            categoryObj["name"] = "USB";
            break;
        case CATEGORY_WIFI:
            categoryObj["name"] = "WIFI";
            break;
        case CATEGORY_TFT:
            categoryObj["name"] = "TFT";
            break;
        default:
            categoryObj["name"] = "UNKNOWN";
            break;
        }

        const auto &settingsArray = categoryObj["settings"].to<JsonArray>();

        for (const auto &setting : settingInCategory)
        {
            const std::string &settingName = get<0>(setting);
            const RedLilyGOCapability::SettingType &settingType = get<1>(setting);
            const std::string &defaultValue = get<2>(setting);

            const auto &settingsJObject = settingsArray.add<JsonObject>();

            settingsJObject["name"] = settingName;
            settingsJObject["type"] = (uint8_t)settingType;
            settingsJObject["default"] = isSensitiveSetting(settingName) ? "********" : defaultValue;

            if (!prefs.isKey(settingName.c_str()))
            {
                settingsJObject["state"] = "unset";
                settingsJObject["value"] = isSensitiveSetting(settingName) ? "********" : defaultValue;
            }
            else
            {
                settingsJObject["state"] = "set";

                switch (settingType)
                {
                case RedLilyGOCapability::SettingType::String:
                    settingsJObject["value"] = isSensitiveSetting(settingName) ? "********" : prefs.getString(settingName.c_str());
                    break;
                case RedLilyGOCapability::SettingType::Bool:
                    settingsJObject["value"] = prefs.getBool(settingName.c_str());
                    break;
                case RedLilyGOCapability::SettingType::Int16:
                    settingsJObject["value"] = prefs.getShort(settingName.c_str());
                    break;
                case RedLilyGOCapability::SettingType::UInt16:
                    settingsJObject["value"] = prefs.getUShort(settingName.c_str());
                default:
                    break;
                };
            }
        }
    }
}

void resetSettings(Preferences &prefs)
{
    for (const auto &settingCategory : settingLookup)
    {
        const std::vector<std::tuple<std::string, RedLilyGOCapability::SettingType, std::string>> &settingInCategory = settingCategory.second;
        for (const auto &setting : settingInCategory)
        {
            const std::string &settingName = get<0>(setting);

            if (prefs.isKey(settingName.c_str()))
            {
                if (prefs.remove(settingName.c_str()))
                {
                    Debug::Log.info(LOG_SETTINGS, std::string("Unset ") + settingName.c_str());
                }
            }
        }
    }
}
