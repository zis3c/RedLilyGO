/** @file IStorage.h @brief Common filesystem and sector-storage contract. */

#pragma once

#include <string>
#include <cstdint>
#include <vector>

#ifdef NO_SD
  #include <SPIFFS.h>
#else
  #include <SD_MMC.h>
#endif

class IStorage {
public:
  virtual ~IStorage() {} // Virtual destructor (important for polymorphism)

  virtual std::vector<std::string> listFiles() = 0;
  virtual size_t getFileSize(const std::string& filename) = 0;
  virtual bool doesFileExist(const std::string& filename) = 0;
  virtual bool createEmptyFile(const std::string &filename) = 0;
  virtual uint8_t* readFileAsBinary(const std::string& filename, size_t& size) = 0;
  virtual std::string readLineFromFile(const std::string &filename, const int lineNumber) = 0;
};
