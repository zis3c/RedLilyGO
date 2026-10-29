/**
 * @file StorageDevice.h
 * @brief Thread-safe filesystem and raw-sector storage abstraction.
 *
 * Supports SD, SD_MMC, and SPIFFS profiles. Operations serialize through the
 * lifecycle mutex and run from the Arduino control path.
 */

#pragma once

#include "../../RedLilyGOCapability.h"
#include "IStorage.h"
#include <atomic>
#include <mutex>

class StorageDevice : IStorage, RedLilyGOCapability {
public:
  /** @brief Construct storage adapter in stopped state. */
  StorageDevice();

  /** @brief Service storage backend; no periodic work required currently. */
  virtual void loop(Preferences& prefs);
  /** @brief Start storage backend using persisted settings. */
  virtual void begin(Preferences& prefs);
  /** @brief Start storage backend, optionally formatting its filesystem. */
  void begin(Preferences& prefs, bool format);

  /** @brief Report whether backend is mounted and usable. */
  bool isRunning() const { return running.load(); }
  bool getUsage(uint64_t &used, uint64_t &total);
  virtual std::vector<std::string> listFiles();
  /** @brief List a bounded set of immediate children without walking the full SD tree. */
  bool listDirectory(const std::string& path, std::vector<std::string>& directories, std::vector<std::string>& files,
                     size_t maxEntries, bool& directoriesTruncated, bool& filesTruncated);
  virtual size_t getFileSize(const std::string& filename);
  virtual bool doesFileExist(const std::string& filename);
  bool ensureStandardDirectories();
  bool isFull();
  static std::string directoryForFilename(const std::string& filename);
  std::string resolveImagePath(const std::string& filename);
  static std::string resolveUploadPath(const std::string& filename, const std::string& directory);
  static bool isSafeUserPath(const std::string& filename);
  virtual bool createEmptyFile(const std::string &filename);
  /** @brief Read owned bytes and their exact length from the same open file. Caller frees the buffer. */
  virtual uint8_t* readFileAsBinary(const std::string& filename, size_t& size);
  virtual std::string readLineFromFile(const std::string &filename, const int lineNumber);
  bool writeFileChunk(const std::string& filename, const uint8_t *buffer, size_t size, bool append);
  bool appendFileQuiet(const std::string& filename, const uint8_t *buffer, size_t size);
  std::string readFileTail(const std::string& filename, size_t maxLines);
  size_t readFileChunk(const std::string& filename, size_t offset, uint8_t *buffer, size_t size);
  virtual bool deleteFile(const std::string& filename);
  bool moveFile(const std::string& source, const std::string& destination);
  bool commitUpload(const std::string& temporary, const std::string& destination, bool replaceExisting);
  virtual File openFile(const std::string& filename, const char* mode);
  virtual bool isRawAccessSupported();
  virtual size_t sectorSize();
  virtual size_t deviceCapacity();
  virtual int32_t readRawSectors(uint8_t* buffer, uint32_t lba, uint32_t sectors);
  virtual int32_t writeRawSectors(uint8_t* buffer, uint32_t lba, uint32_t sectors);
  virtual void flush();
  virtual void refreshCache();
  std::recursive_mutex &lifecycleMutex() { return storageMutex; }
private:
  std::atomic<bool> running{false};
  mutable std::recursive_mutex storageMutex;
};

namespace Devices
{
    extern StorageDevice Storage;
}
