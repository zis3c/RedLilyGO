/** @file RequestValidation.h @brief Stateless HTTP input validation and file classification. */
#pragma once

#include <Arduino.h>
#include <cctype>

namespace WebRequestValidation {
static constexpr size_t MAX_COMMAND_LENGTH = 1024;

inline bool validUsername(const String &value)
{
  if (value.length() < 3 || value.length() > 32) return false;
  for (size_t i = 0; i < value.length(); ++i)
    if (!isalnum(static_cast<unsigned char>(value[i])) && value[i] != '_' && value[i] != '-') return false;
  return true;
}

inline bool validStoragePath(const String &value)
{
  if (value.isEmpty() || value.length() > 128 || value.indexOf("..") >= 0 || value.indexOf('\\') >= 0) return false;
  for (size_t i = 0; i < value.length(); ++i)
    if (static_cast<unsigned char>(value[i]) < 0x20 ||
        static_cast<unsigned char>(value[i]) == 0x7f || value[i] == '"') return false;
  return value[0] == '/';
}

inline bool validUploadFilename(const String &value)
{
  if (value.isEmpty() || value.length() > 128 || value.indexOf("..") >= 0 ||
      value.indexOf('/') >= 0 || value.indexOf('\\') >= 0) return false;
  for (size_t i = 0; i < value.length(); ++i)
    if (static_cast<unsigned char>(value[i]) < 0x20 || value[i] == 0x7f) return false;
  return true;
}

inline bool hasExtensionIgnoreCase(const String &value, const char *extension)
{
  String lowered = value;
  lowered.toLowerCase();
  return lowered.endsWith(extension);
}

inline String fileTypeForPath(const String &path)
{
  const int dot = path.lastIndexOf('.');
  if (dot < 0) return "other";
  String ext = path.substring(dot);
  ext.toLowerCase();
  if (ext == ".html" || ext == ".htm") return "html";
  if (ext == ".ds") return "ducky";
  if (ext == ".pcap" || ext == ".pcapng" || ext == ".csv" || ext == ".log" || ext == ".jsonl") return "capture";
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif") return "image";
  return "other";
}

inline bool validCommand(const String &value)
{
  if (value.isEmpty() || value.length() > MAX_COMMAND_LENGTH) return false;
  bool hasCommandCharacter = false;
  for (size_t i = 0; i < value.length(); ++i)
  {
    const unsigned char character = static_cast<unsigned char>(value[i]);
    if (character < 0x20 || character == 0x7f) return false;
    if (!isspace(character)) hasCommandCharacter = true;
  }
  return hasCommandCharacter;
}

inline bool isMarauderCommand(String value)
{
  value.trim();
  const int separator = value.indexOf(' ');
  if (separator >= 0) value = value.substring(0, separator);
  value.toLowerCase();
  static const char *const commands[] = {
    "attack", "scanap", "scansta", "sniffraw", "sniffbeacon", "sniffprobe",
    "sniffpwn", "sniffesp", "sniffdeauth", "sniffpmkid", "stopscan", "sigmon",
    "evilportal", "channel", "clearlist", "reboot", "settings", "ls", "led",
    "gpsdata", "gps", "nmea", "list", "select", "ssid", "save", "load",
    "blespam", "sniffbt", "btwardrive", "sniffskim", "wardrive"
  };
  for (const char *command : commands)
  {
    if (value == command) return true;
  }
  return false;
}

inline bool validWifiText(const String &value, const size_t maxLength)
{
  if (value.isEmpty() || value.length() > maxLength) return false;
  bool hasVisibleCharacter = false;
  for (size_t i = 0; i < value.length(); ++i)
  {
    const unsigned char character = static_cast<unsigned char>(value[i]);
    if (character < 0x20 || character == 0x7f) return false;
    if (!isspace(character)) hasVisibleCharacter = true;
  }
  return hasVisibleCharacter;
}

} // namespace WebRequestValidation
