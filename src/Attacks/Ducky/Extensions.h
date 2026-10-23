/** @file Extensions.h @brief Ducky interpreter command extension declarations. */

#include "Adafruit_TinyUSB.h"

#include "Diagnostics/Logging.h"

#include "../../Devices/Button/ButtonDevice.h"
#include "../../Devices/Storage/StorageDevice.h"
#include "../../Devices/USB/USBCore.h"
#include "../../Devices/USB/USBHID.h"
#include "../../Devices/USB/USBMSC.h"
#include "../../Devices/USB/USBNCM.h"
#include "../../Devices/Display/DisplayDevice.h"
#include "../../Devices/LED/LEDDevice.h"
#include "../../Devices/WiFi/WiFiDevice.h"
#include "../../Devices/Touch/TouchDevice.h"
#include "../../Devices/Platform/BoardSupport.h"
#include "../../Comms/Web/WebServer.h"

#include "../../Attacks/Marauder/Marauder.h"
#include "../../Attacks/Agent/Agent.h"

#include "../../Utilities/Settings.h"

#include "DuckyPayload.h"

#include <stdexcept>

// Helpers used to expose the USB VID and PID as DuckyScript constants.
static std::string vidStr;
static const std::string Constant_VID = "#_VID_";
static std::string pidStr;
static const std::string Constant_PID = "#_PID_";
static const std::string Constant_FileIndexFileName = "#_FILE_INDEX_FILE_NAME_";
static const std::string Constant_CurrentIP = "#_CURRENT_IP_";

// Helpers used by payload file-index and SD-file-list commands.
static std::vector<std::string> curListOfFiles;
static int curFileIndexVariable = 0;
static std::string curCachedIP;

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

static int handleCalc(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::USB::HID.consumer_device_keypress(HID_USAGE_CONSUMER_AL_CALCULATOR);
    return true;
}

static int handleDisplayClear(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::Display.clearScreen();
    return true;
}

static int handleTFT_OFF(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::Display.powerOff();
    return true;
}

static int handleTFT_ON(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::Display.powerOn();
    return true;
}

static int handleDisplayPNG(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string arg = str.substr(str.find(' ') + 1);
    const std::string imagePath = Devices::Storage.resolveImagePath(arg);
    if (imagePath.empty())
    {
        Debug::Log.error(LOG_DUCKY, "DISPLAY_PNG file unavailable");
        return false;
    }
    Devices::Display.displayPng(Devices::Storage, imagePath);
    return true;
}

static int asciiOrVariableToInt(const std::string &input, const std::unordered_map<std::string, int> &variables)
{
    if (variables.find(input) == variables.cend())
    {
        return atoi(input.c_str());
    }
    else
    {
        return variables.at(input);
    }
}

static int handleDisplayText(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string remainingArgs = str.substr(str.find(' ') + 1);
    const auto firstSpace = remainingArgs.find(' ');
    const auto secondSpace = firstSpace == std::string::npos ? std::string::npos : remainingArgs.find(' ', firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos || secondSpace + 1 >= remainingArgs.size())
    {
        Debug::Log.error(LOG_DUCKY, "DISPLAY_TEXT requires X, Y, and text");
        return false;
    }

    auto xPosStr = remainingArgs.substr(0, firstSpace);
    int xPos = asciiOrVariableToInt(xPosStr, variables);

    auto yStartPos = xPosStr.length() + 1;
    auto yCount = secondSpace - yStartPos;

    auto yPosStr = remainingArgs.substr(yStartPos, yCount);
    int yPos = asciiOrVariableToInt(yPosStr, variables);

    auto text = remainingArgs.substr(xPosStr.length() + yPosStr.length() + 2);

    for (const auto& pair : variables)
    {
        text = Ducky::replaceAllOccurrences(text, pair.first, std::to_string(pair.second));
    }

    for (const auto& pair : constants)
    {
        text = Ducky::replaceAllOccurrences(text, pair.first, pair.second);
    }

    Devices::Display.display(xPos, yPos, text);
    return true;
}

static int handleDisplaySetTextSize(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    const std::string arg = str.substr(str.find(' ') + 1);
    const auto entries = Ducky::SplitString(arg);

    if (entries.size() != 1)
    {
        Debug::Log.error(LOG_DUCKY, "Bad argument count: " + std::to_string(entries.size()));
        return false;
    }

    const int textSize = asciiOrVariableToInt(arg, variables);
    if (textSize <= 0)
    {
        Debug::Log.error(LOG_DUCKY, "DISPLAY_SET_TEXT_SIZE arg must be > 0, units of 0.1x");
        return false;
    }

    Devices::Display.setTextSize(textSize);
    return true;
}


static int handleUSBMode(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    bool mountAsCdrom = startsWith(str, "USB_MOUNT_CDROM_READ_ONLY");

    std::string arg = str.substr(str.find(' ') + 1);
    const auto entries = Ducky::SplitString(arg);

    if (entries.size() == 1)
    {
        Debug::Log.info(LOG_DUCKY, "Mounting disk requested");
        return Devices::USB::MSC.mountDiskImage(entries[0], mountAsCdrom);
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "Bad argument count: " + std::to_string(entries.size()));
    }
    return false;
}

static int handleLED(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string arg = str.substr(str.find(' ') + 1);

    const auto entries = Ducky::SplitString(arg);
    if (entries.size() == 4)
    {
        auto hue = asciiOrVariableToInt(entries[0], variables);
        auto sat = asciiOrVariableToInt(entries[1], variables);
        auto lum = asciiOrVariableToInt(entries[2], variables);
        auto brightness = asciiOrVariableToInt(entries[3], variables);

        if (hue < 0 || hue > UINT8_MAX || sat < 0 || sat > UINT8_MAX ||
            lum < 0 || lum > UINT8_MAX || brightness < 0 || brightness > UINT8_MAX)
        {
            Debug::Log.info(LOG_DUCKY, "LED value out of range");
            return false;
        }

        Devices::LED.changeLEDState(true, hue, sat, lum, brightness);
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "LED command has invalid arguments.");
        return false;
    }

    return true;
}

static int handleLEDBlue(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::LED.changeLEDState(true, 240, 100, 100, 255);
    return true;
}

static int handleFileExists(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    // The parser stores the requested path in the #FILE constant.
    if (constants.find("#FILE") != constants.cend())
    {
        auto ret = Devices::Storage.doesFileExist(constants.at("#FILE"));

        Debug::Log.info(LOG_DUCKY, "FILE_EXISTS() returned " + std::to_string(ret));
        return ret;
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "FILE_EXISTS requires a parameter.");
        return false;
    }
}

static int handleCreateFile(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    // The parser stores the requested path in the #FILE constant.
    if (constants.find("#FILE") != constants.cend())
    {
        return Devices::Storage.createEmptyFile(constants.at("#FILE"));
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "CREATE_FILE requires a parameter.");
        return false;
    }
}

static int handleESP32Marauder(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string arg = str.substr(str.find(' ') + 1);
    if (!Attacks::Marauder.isRunning())
    {
        auto *prefs = Attacks::Ducky.getPreferences();
        if (prefs == nullptr)
        {
            Debug::Log.error(LOG_DUCKY, "ESP32M command failed: settings unavailable");
            return false;
        }
        if (Devices::Board.isSafeMode())
        {
            Debug::Log.error(LOG_DUCKY, "ESP32M command blocked in safe mode");
            return false;
        }
        Attacks::Marauder.begin(*prefs);
        if (!Attacks::Marauder.isRunning())
        {
            Debug::Log.error(LOG_DUCKY, "ESP32M command failed: Marauder startup failed");
            return false;
        }
    }
    Attacks::Marauder.run(arg);
    return true;
}

static int handleWebOff(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Comms::Web.end();
    return true;
}

static int handleWiFiOff(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::Wifi.setWiFi(false);
    return true;
}

static int handleWiFiOn(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::Wifi.setWiFi(true);
    return true;
}

static int handleSerial(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string arg = str.substr(str.find(' ') + 1);
    auto speed = asciiOrVariableToInt(arg.c_str(), variables);
    if (speed <= 0)
    {
        Debug::Log.error(LOG_DUCKY, "Invalid serial baud rate");
        return false;
    }
    Serial.begin(speed);

    return true;
}

static int handleSetSetting(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    bool ret = false;

    do
    {
        const auto split = Ducky::SplitString(str);
        if (split.size() != 3)
        {
            Debug::Log.error(LOG_DUCKY, "SET_SETTING needs two arguments has: "+std::to_string(split.size() - 1));
            Debug::Log.error(LOG_DUCKY, "SET_SETTING command has invalid argument count");
            totalErrors++;
            break;
        }

        const auto settingName = split[1];
        auto settingValue = split[2];

        if (startsWith(str, "SET_SETTING_UINT16") && settingValue.rfind("0x", 0) != 0)
        {
            // Preserve the hexadecimal setting format expected by the parser.
            settingValue = "0x" + settingValue;
        }

        if (startsWith(str, "SET_SETTING_BOOL"))
        {
            if (settingValue == "0" || settingValue == "1" || settingValue == "TRUE" || settingValue == "FALSE")
            {
                // Normalize accepted boolean spellings to 0 or 1.
                if (settingValue == "TRUE" || settingValue == "1")
                {
                    settingValue = "1";
                }
                else
                {
                    settingValue = "0";
                }
            }
            else
            {
                Debug::Log.error(LOG_DUCKY, "Invalid bool setting value");
                totalErrors++;
                break;
            }
        }

        Debug::Log.info(LOG_DUCKY, "Trying to update a setting");
        ret = setSettingValue(*preferences, settingName, settingValue);

    } while (false);

    return ret;
}

static int handleUsbNcmPcapOn(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    return Devices::USB::NCM.startPacketCollection();
}

static int handleUsbNcmPcapOff(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::USB::NCM.stopPacketCollection();
    return true;
}

static int handleAgentRun(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string arg = str.substr(str.find(' ') + 1);
    if (arg.empty() || !Attacks::Agent.isAgentConnected() || !Attacks::Agent.protocolCompatible())
    {
        Debug::Log.error(LOG_DUCKY, "AGENT_RUN unavailable or incompatible");
        return false;
    }
    Attacks::Agent.run(arg);
    return true;
}

static int handleWaitForhandleAgentRunResult(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Debug::Log.info(LOG_DUCKY, "Waiting for agent result.");
    startWait(WaitKind::AgentResult, 30000);
    return true;
}

static int handleDeleteFile(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    // The parser stores the requested path in the #FILE constant.
    if (constants.find("#FILE") != constants.cend())
    {
        auto ret = Devices::Storage.deleteFile(constants.at("#FILE"));

        Debug::Log.info(LOG_DUCKY, "DELETE_FILE() returned " + std::to_string(ret));
        return ret;
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "DELETE_FILE requires a parameter.");
        return false;
    }
}

static int handleAgentConnected(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    return Attacks::Agent.isAgentConnected();
}

// Load all files, load only DuckyScript files, or validate a selected file index.
static int handleFileIndex(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    bool ret = false;

    if (str == "LOAD_FILES_FROM_SD()")
    {
        curListOfFiles = Devices::Storage.listFiles();
        ret = true;
    }
    else if (str == "LOAD_DS_FILES_FROM_SD()")
    {
        std::vector<std::string> allFiles = Devices::Storage.listFiles();
        curListOfFiles.clear();

        for (const auto& file : allFiles)
        {
            if (file.size() >= 3 && file.substr(file.size() - 3) == ".ds")
            {
                curListOfFiles.push_back(file);
            }
        }

        ret = true;
    }
    else
    {
        // Validate the current selection against the cached file list.
        const std::string varName = "$FILE_INDEX";

        if (variables.find(varName) == variables.cend())
        {
            Debug::Log.info(LOG_DUCKY, "Variable not found.");
            ret = false;
        }
        else
        {
            curFileIndexVariable = variables.at(varName);
            if (curFileIndexVariable >= curListOfFiles.size())
            {
                ret = false;
            }
            else
            {
                ret = true;
            }
        }
    }

    return ret;
}

static int handleButtonPress(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Debug::Log.info(LOG_DUCKY, "Button press handled");
    if (str == "BUTTON_LONG_PRESS()")
    {
        return lastButtonPressState == 1;
    }
    else
    {
        return lastButtonPressState == 0;
    }
}

static int handleGetSettingValue(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    // The parser stores the setting name in the #SETTING_NAME constant.
    if (constants.find("#SETTING_NAME") != constants.cend())
    {
        bool error = false;
        uint16_t value = getIntegerSettingValue(*preferences, constants.at("#SETTING_NAME"), error);
        if (error)
        {
            // The extension callback has no interpreter error channel, so preserve the
            // existing return contract and count the failure for the runtime log.
            Debug::Log.error(LOG_DUCKY, "GET_SETTING_VALUE() error, setting is unknown or string type");
            totalErrors++;
            return 0;
        }
        Debug::Log.info(LOG_DUCKY, "GET_SETTING_VALUE completed");
        return value;
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "GET_SETTING_VALUE requires a parameter.");
        return false;
    } 
}

static int handleGetRecvPackets(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    return Attacks::Marauder.getPacketCount();
}

static int handleRunPayload(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    const std::string arg = str.substr(str.find(' ') + 1);
    if (arg.ends_with(".ds"))
    {
        Attacks::Ducky.setPayload(arg);
    }
    else if (arg.ends_with(".png"))
    {
        Attacks::Ducky.setPayload("");
        Attacks::Ducky.setPayloadCmdLine("DISPLAY_PNG " + arg);
    }

    return true;
}

static int handleWaitForUSBStorageActivity(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Debug::Log.info(LOG_DUCKY, "Waiting for MSC activity.");
    startWait(WaitKind::MscActivity, 30000);
    return true;
}

static int handleWaitForUSBStorageActivityToStop(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Debug::Log.info(LOG_DUCKY, "Waiting 1000 ms for MSC activity to stop.");
    startWait(WaitKind::MscIdle, 60000);
    return true;
}

static int handleWaitForButtonOrTimeout(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    const std::string arg = str.substr(str.find(' ') + 1);
    auto timeout = asciiOrVariableToInt(arg, variables);

    waitForButtonWithTimeout(timeout);
    return true;
}

static int handleRawHid(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    bool ret = false;

    const std::string arg = str.substr(str.find(' ') + 1);
    const auto words = Ducky::SplitString(arg);

    if (words.size() == 0 || words.size() > 2)
    {
        Debug::Log.info(LOG_DUCKY, "Invalid syntax.");
        return false;
    }

    const auto hidStr = words.size() == 1 ? words[0] : words[1];
    const auto modStr = words.size() == 1 ? std::string() : words[0];

    if (hidStr.rfind("0x", 0) == 0)
    {
    // Catch malformed or oversized numbers here so conversion errors do not escape
    // into the interpreter and stop the firmware.
        int tempInt = -1;
        try
        {
            tempInt = std::stoi(hidStr, nullptr, 16);
        }
        catch (const std::exception&)
        {
            Debug::Log.error(LOG_DUCKY, "Invalid value.");
            return false;
        }

        if (tempInt <= static_cast<int>(UINT8_MAX) && tempInt >= 0)
        {
            uint8_t uint8Value = 0;
            uint8Value = static_cast<uint8_t>(tempInt);

            Debug::Log.info(LOG_DUCKY, "Pressing key " + std::to_string(uint8Value));

            uint8_t modKey = modStr == "SHIFT" ? (uint8_t)USBKeyDefinition::UsbHidModifiers::LeftShift : 0;

            Devices::USB::HID.keyboard_press(modKey, uint8Value, 0, 0, 0, 0, 0);
            Devices::USB::HID.keyboard_release();

            ret = true;
        }
        else
        {
            Debug::Log.error(LOG_DUCKY, "Invalid value.");
        }
    }
    else
    {
        Debug::Log.error(LOG_DUCKY, "Invalid value. It must start with 0x.");
    }

    return ret;
}

static int handleLog(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    std::string text = str.substr(str.find(' ') + 1);

    for (const auto& pair : variables)
    {
        text = Ducky::replaceAllOccurrences(text, pair.first, std::to_string(pair.second));
    }

    for (const auto& pair : constants)
    {
        text = Ducky::replaceAllOccurrences(text, pair.first, pair.second);
    }

    Debug::Log.info(LOG_DUCKY, text);
    return true;
}

static int handleKeyboardLayout(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    const std::string arg = str.substr(str.find(' ') + 1);

    const auto entries = Ducky::SplitString(arg);
    if (entries.size() == 1)
    {
        const auto &ret = duckyFileParser.SetKeyboardLayout(entries[0]);

        if (!ret)
        {
            Debug::Log.error(LOG_DUCKY, "Invalid keyboard layout. It may not be included in this build.");
        }
        else
        {
            Debug::Log.info(LOG_DUCKY, "Keyboard layout updated");
            return true;
        }
    }
    else
    {
        Debug::Log.error(LOG_DUCKY, "Invalid keyboard layout. Too many tokens.");
    }

    return false;
}

static int waitForTouch(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Debug::Log.info(LOG_DUCKY, "Waiting for touch input.");
    startWait(WaitKind::Touch, 30000);
    return true;
}

static int getTouchPosition(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    const auto pos = startsWith(str, "GET_X_POS") ? Devices::Touch.getXPos() : Devices::Touch.getYPos();
    Debug::Log.info(LOG_DUCKY, "Touch position read");
    return pos;
}

static int handleMouseJiggle(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::USB::HID.mouseMove(1, 1);
    Devices::USB::HID.mouseMove(-1, -1);
    return true;
}

static int handleMouseMove(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    const std::string arg = str.substr(str.find(' ') + 1);

    const auto entries = Ducky::SplitString(arg);
    if (entries.size() == 2)
    {
        int xDelta = asciiOrVariableToInt(entries[0], variables);
        int yDelta = asciiOrVariableToInt(entries[1], variables);
        if (xDelta < INT8_MIN || xDelta > INT8_MAX || yDelta < INT8_MIN || yDelta > INT8_MAX)
        {
            Debug::Log.info(LOG_DUCKY, "Mouse delta out of range");
            return false;
        }
        Devices::USB::HID.mouseMove(xDelta, yDelta);
        return true;
    }
    else
    {
        Debug::Log.info(LOG_DUCKY, "Bad argument count: " + std::to_string(entries.size()));
        return false;
    }
}

static int handleIsWiFiConnected(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    return Devices::Wifi.getState();
}

static int handleResetSettings(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    resetSettings(*preferences);
    return true;
}

static int handleUsbReset(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    Devices::USB::Core.reset();
    return true;
}

static int handleHasCrashed(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
{
    return Devices::Board.hasCrashed();
}

void addDuckyScriptExtensions(
    ExtensionCommands &extCommands,
    UserDefinedConstants &consts)
{
    // Commands perform actions; functions return values and may also have side effects.

    // Display and user-interface commands.
    extCommands["TFT_OFF"] = handleTFT_OFF;
    extCommands["TFT_ON"] = handleTFT_ON;
    extCommands["DISPLAY_PNG"] = handleDisplayPNG;
    extCommands["DISPLAY_TEXT"] = handleDisplayText;
    extCommands["DISPLAY_CLEAR"] = handleDisplayClear;
    extCommands["DISPLAY_SET_TEXT_SIZE"] = handleDisplaySetTextSize;
    extCommands["LED"] = handleLED;
    extCommands["LED_B"] = handleLEDBlue;

    // Touch input commands.
    extCommands["WAIT_FOR_TOUCH"] = waitForTouch;
    extCommands["GET_X_POS()"] = getTouchPosition;
    extCommands["GET_Y_POS()"] = getTouchPosition;

    // Other feature and assessment commands.
    extCommands["CALC"] = handleCalc;
    extCommands["ESP32M"] = handleESP32Marauder;
    extCommands["RUN_PAYLOAD"] = handleRunPayload;
    extCommands["RAW_HID"] = handleRawHid;

    // General-purpose helpers.
    extCommands["LOG"] = handleLog;

    // Device and network controls.
    extCommands["WEB_OFF"] = handleWebOff;
    extCommands["WIFI_OFF"] = handleWiFiOff;
    extCommands["WIFI_ON"] = handleWiFiOn;
    extCommands["SERIAL"] = handleSerial;
    extCommands["SET_SETTING_BOOL"] = handleSetSetting;
    extCommands["SET_SETTING_INT16"] = handleSetSetting;
    extCommands["SET_SETTING_UINT16"] = handleSetSetting;
    extCommands["SET_SETTING_STRING"] = handleSetSetting;
    extCommands["RESET_SETTINGS"] = handleResetSettings;

    // USB mode and input controls.
    extCommands["USB_MOUNT_DISK_READ_ONLY"] = handleUSBMode;
    extCommands["USB_MOUNT_CDROM_READ_ONLY"] = handleUSBMode;
    extCommands["USB_NCM_PCAP_ON"] = handleUsbNcmPcapOn;
    extCommands["USB_NCM_PCAP_OFF"] = handleUsbNcmPcapOff;
    extCommands["USB_RESET"] = handleUsbReset;
    extCommands["WAIT_FOR_USB_STORAGE_ACTIVITY"] = handleWaitForUSBStorageActivity;
    extCommands["WAIT_FOR_USB_STORAGE_ACTIVITY_TO_STOP"] = handleWaitForUSBStorageActivityToStop;
    extCommands["WAIT_FOR_BUTTON_PRESS_OR_TIMEOUT"] = handleWaitForButtonOrTimeout;
    extCommands["MOUSE_JIGGLE"] = handleMouseJiggle;
    extCommands["MOUSE_MOVE"] = handleMouseMove;

    extCommands["KEYBOARD_LAYOUT"] = handleKeyboardLayout;

    // Windows Agent commands.
    extCommands["AGENT_RUN"] = handleAgentRun;
    extCommands["WAIT_FOR_AGENT_RUN_RESULT"] = handleWaitForhandleAgentRunResult;

    // Value-returning DuckyScript functions.
    extCommands["FILE_EXISTS()"] = handleFileExists;
    extCommands["CREATE_FILE()"] = handleCreateFile;
    extCommands["DELETE_FILE()"] = handleDeleteFile;
    extCommands["AGENT_CONNECTED()"] = handleAgentConnected;
    extCommands["FILE_INDEX_VALID()"] = handleFileIndex;
    extCommands["LOAD_FILES_FROM_SD()"] = handleFileIndex;
    extCommands["LOAD_DS_FILES_FROM_SD()"] = handleFileIndex;
    extCommands["BUTTON_LONG_PRESS()"] = handleButtonPress;
    extCommands["BUTTON_SHORT_PRESS()"] = handleButtonPress;
    extCommands["GET_SETTING_VALUE()"] = handleGetSettingValue;
    extCommands["ESP32M_GET_RECV_PACKETS()"] = handleGetRecvPackets;
    extCommands["WIFI_CONNECTED()"] = handleIsWiFiConnected;
    extCommands["HAS_CRASHED()"] = handleHasCrashed;

    // clang-format off
    consts.emplace_back([]
    {
        if (vidStr.empty())
        {
            char hexString[5] = {0};
            snprintf(hexString, sizeof(hexString), "%x", Devices::USB::Core.getVID()); // DuckyScript stores USB IDs as hexadecimal text.
            vidStr = std::string(hexString);
        }
        return std::pair(Constant_VID, vidStr);
    });
    consts.emplace_back([]
    {
        if (pidStr.empty())
        {
            char hexString[5] = {0};
            snprintf(hexString, sizeof(hexString), "%x", Devices::USB::Core.getPID()); // DuckyScript stores USB IDs as hexadecimal text.
            pidStr = std::string(hexString);
        }
        return std::pair(Constant_PID, pidStr); 
    });
    consts.emplace_back([]
    {
        // The index comes from the user-controlled $FILE_INDEX variable, so it
        // must be bounds-checked here or a stale/out-of-range value indexes
        // past the end of the vector (undefined behaviour / crash).
        if (curFileIndexVariable >= 0 && static_cast<size_t>(curFileIndexVariable) < curListOfFiles.size())
        {
            return std::pair(Constant_FileIndexFileName, curListOfFiles[static_cast<size_t>(curFileIndexVariable)]);
        }
        else
        {
            return std::pair(Constant_FileIndexFileName, std::string());
        }
    });
    consts.emplace_back([]
    { 
        if (Devices::Wifi.getState())
        {
            if (curCachedIP.empty())
            {
                curCachedIP = Devices::Wifi.currentIPAddress();
            }   
        }
        else
        {
            curCachedIP.clear();
        }

        return std::pair(Constant_CurrentIP, curCachedIP);
    });
    // clang-format on
}
