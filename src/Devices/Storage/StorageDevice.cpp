/** @file StorageDevice.cpp @brief Storage backend selection and filesystem lifecycle. */

#include "StorageDevice.h"

#include "Diagnostics/Logging.h"
#include <limits>
#include <algorithm>
#include <cctype>
#include <deque>

namespace {
constexpr std::size_t MAX_BINARY_READ_BYTES = 1024 * 1024;
constexpr const char *STANDARD_DIRECTORIES[] = {
    "/Portal", "/Scripts", "/Captures", "/Captures/WiFi",
    "/Captures/Bluetooth", "/Captures/Other", "/Captures/Portal", "/Images", "/Other", "/Logs"
};

std::string lowerExtension(const std::string &filename)
{
    const auto slash = filename.find_last_of('/');
    const auto dot = filename.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    std::string extension = filename.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

std::string lowerFilename(const std::string &filename)
{
    const auto slash = filename.find_last_of('/');
    std::string basename = slash == std::string::npos ? filename : filename.substr(slash + 1);
    std::transform(basename.begin(), basename.end(), basename.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return basename;
}
}

#ifdef NO_SD
    #include <SPIFFS.h>
    #define FILE_INTERFACE SPIFFS
#elif USE_SD_INTERFACE
    // Use the Arduino SD library when this board profile selects SPI storage.
    #include "SD.h"
    using namespace fs;
    #define FILE_INTERFACE SD
#else
    #include "ESP32/SDMmcFileSystem.h"
    #include "driver/sdmmc_host.h"
    #include "driver/sdspi_host.h"
    #include "sdmmc_cmd.h"
    using namespace fs;
    #define FILE_INTERFACE SD_MMC_2
#endif

#define LOG_MMC "MMC"

static std::vector<std::string> filesCache;

namespace Devices
{
    StorageDevice Storage;
}

bool StorageDevice::writeFileChunk(const std::string& filename, const uint8_t *buffer, const size_t size, const bool append)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename) || (size != 0 && buffer == nullptr))
    {
        Debug::Log.info(LOG_MMC, "Storage unavailable or invalid write buffer.");
        return false;
    }

    File file = FILE_INTERFACE.open(filename.c_str(), append ? "a" : "w");
    if (!file)
    {
        Debug::Log.info(LOG_MMC, "Could not open file.");
        return false;
    }

    const bool success = file.write(buffer, size) == size;
    if (!success)
    {
        Debug::Log.info(LOG_MMC, "File write failed.");
    }

    file.close();
    refreshCache();
    return success;
}

size_t StorageDevice::readFileChunk(const std::string& filename, const size_t offset, uint8_t *buffer, const size_t size)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename) || buffer == nullptr || size == 0) return 0;
    File file = FILE_INTERFACE.open(filename.c_str(), "r");
    if (!file || !file.seek(offset)) return 0;
    const size_t read = file.read(buffer, size);
    file.close();
    return read;
}

File StorageDevice::openFile(const std::string& filename, const char* mode)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename) || mode == nullptr || mode[0] == '\0') return File();
    File file = FILE_INTERFACE.open(filename.c_str(), mode);
    if (!file)
    {
        Debug::Log.error(LOG_MMC, "Could not open file.");
        return File();
    }

    return file;
}

std::size_t StorageDevice::getFileSize(const std::string& filename)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename)) return 0;
    File file = FILE_INTERFACE.open(filename.c_str(), "r");
    if (!file) return 0;

    // Check the size before allocating a binary read buffer.
    const std::size_t ret = file.size();
    file.close();
    return ret == static_cast<std::size_t>(-1) ? 0 : ret;
}

bool StorageDevice::doesFileExist(const std::string& filename)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename)) return false;
    return FILE_INTERFACE.exists(filename.c_str());
}

bool StorageDevice::ensureStandardDirectories()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running) return false;
    bool success = true;
    for (const char *directory : STANDARD_DIRECTORIES)
    {
        if (!FILE_INTERFACE.exists(directory) && !FILE_INTERFACE.mkdir(directory)) success = false;
    }
    filesCache.clear();
    return success;
}

bool StorageDevice::isFull()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running) return false;
    const uint64_t total = FILE_INTERFACE.totalBytes();
    return total != 0 && FILE_INTERFACE.usedBytes() >= total;
}

bool StorageDevice::appendFileQuiet(const std::string& filename, const uint8_t *buffer, const size_t size)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename) || (size != 0 && buffer == nullptr)) return false;
    if (!FILE_INTERFACE.exists("/Logs") && !FILE_INTERFACE.mkdir("/Logs")) return false;
    if (!FILE_INTERFACE.exists("/Captures") && !FILE_INTERFACE.mkdir("/Captures")) return false;
    File file = FILE_INTERFACE.open(filename.c_str(), "a");
    if (!file) return false;
    const bool success = file.write(buffer, size) == size;
    file.flush();
    file.close();
    return success;
}

std::string StorageDevice::readFileTail(const std::string& filename, const size_t maxLines)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename) || maxLines == 0) return {};
    File file = FILE_INTERFACE.open(filename.c_str(), "r");
    if (!file) return {};

    const size_t fileSize = file.size();
    const size_t window = 128 * 1024;
    const size_t start = fileSize > window ? fileSize - window : 0;
    if (!file.seek(start)) { file.close(); return {}; }
    if (start != 0) file.readStringUntil('\n');

    std::deque<std::string> lines;
    while (file.available())
    {
        String line = file.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        lines.emplace_back(line.c_str());
        while (lines.size() > maxLines) lines.pop_front();
    }
    file.close();

    std::string output;
    for (const auto &line : lines)
    {
        if (!output.empty()) output.push_back('\n');
        output += line;
    }
    return output;
}

std::string StorageDevice::directoryForFilename(const std::string& filename)
{
    const std::string extension = lowerExtension(filename);
    const std::string basename = lowerFilename(filename);
    if (extension == ".html" || extension == ".htm") return "/Portal";
    if (extension == ".ds") return "/Scripts";
    if (extension == ".pcap" || extension == ".pcapng" || extension == ".csv" ||
        extension == ".log" || extension == ".jsonl")
    {
        if (basename == "system.log") return "/Logs";
        if (basename.rfind("evil_portal", 0) == 0) return "/Captures/Portal";
        if (basename.rfind("bt_", 0) == 0 || basename.rfind("ble_", 0) == 0)
            return "/Captures/Bluetooth";
        return "/Captures/WiFi";
    }
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
        extension == ".gif") return "/Images";
    return "/Other";
}

std::string StorageDevice::resolveImagePath(const std::string& filename)
{
    if (filename.empty()) return {};
    const std::string extension = lowerExtension(filename);
    if (extension != ".png") return {};

    const std::string exact = filename.front() == '/' ? filename : "/" + filename;
    if (isSafeUserPath(exact) && doesFileExist(exact)) return exact;

    const auto slash = exact.find_last_of('/');
    const std::string basename = slash == std::string::npos ? exact : exact.substr(slash + 1);
    const std::string imagePath = "/Images/" + basename;
    return isSafeUserPath(imagePath) && doesFileExist(imagePath) ? imagePath : std::string();
}

bool StorageDevice::isSafeUserPath(const std::string& filename)
{
    if (filename.empty() || filename.size() > 128 || filename.front() != '/' ||
        filename.find("..") != std::string::npos || filename.find('\\') != std::string::npos)
        return false;
    for (unsigned char character : filename)
        if (character < 0x20 || character == 0x7f || character == '"') return false;
    return true;
}

std::string StorageDevice::resolveUploadPath(const std::string& filename, const std::string& directory)
{
    if (filename.empty() || filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)
        return {};
    const std::string target = directory.empty() || directory == "Auto"
        ? directoryForFilename(filename)
        : directory;
    const bool allowed = target == "/Portal" || target == "/Scripts" || target == "/Captures" ||
                         target == "/Captures/WiFi" || target == "/Captures/Bluetooth" ||
                         target == "/Captures/Portal" || target == "/Images" ||
                         target == "/Other" || target == "/Logs";
    if (!allowed) return {};
    const std::string path = target + "/" + filename;
    return isSafeUserPath(path) ? path : std::string();
}

uint8_t* StorageDevice::readFileAsBinary(const std::string& filename, size_t& size)
{
    size = 0;
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename)) return nullptr;
    File file = FILE_INTERFACE.open(filename.c_str(), "r");
    if (!file) return 0;

    size_t fileSize = file.size();
    if (fileSize == 0 || fileSize > MAX_BINARY_READ_BYTES || fileSize > std::numeric_limits<size_t>::max() - 1)
    {
        Debug::Log.info(LOG_MMC, "Requested binary file exceeds the read limit.");
        file.close();
        return nullptr;
    }

    // Do not let a file buffer consume the heap needed by USB and Wi-Fi.
    const size_t freeHeap = ESP.getFreeHeap();
    if (fileSize + 1 > freeHeap / 2)
    {
        Debug::Log.info(LOG_MMC, std::string("Binary skip ") + filename +
            " size=" + std::to_string(fileSize) + "B free=" +
            std::to_string(freeHeap) + "B");
        file.close();
        return nullptr;
    }

    // Allocate the bounded buffer used by the binary file reader.
    uint8_t *fileContent = (uint8_t *)malloc(fileSize + 1); // +1 for null terminator
    if (!fileContent)
    {
        Debug::Log.info(LOG_MMC, "Could not allocate memory.");
        file.close();
        return 0;
    }

    // Read only the bytes that fit in the allocated buffer.
    if (file.read((uint8_t *)fileContent, fileSize) != fileSize)
    {
        Debug::Log.info(LOG_MMC, "Could not read file.");
        free(fileContent);
        file.close();
        return 0;
    }

    file.close();

    // Reserve one trailing zero byte for APIs that expect a C-style buffer.
    fileContent[fileSize] = '\0';
    size = fileSize;
    return fileContent;
}

std::string StorageDevice::readLineFromFile(const std::string &filename, const int lineNumber)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename) || lineNumber < 0) return "";
    File file = FILE_INTERFACE.open(filename.c_str(), "r");
    if (!file) return "";

    int currentLine = 0;
    String line;
    bool found = false;
    while (file.available())
    {
        line = file.readStringUntil('\n'); // Read one logical line, including the final unterminated line.
        if (currentLine == lineNumber)
        {
            found = true;
            break;
        }
        line = "";
        currentLine++;
    }

    std::string ret = std::string(line.c_str());
    file.close();

    // Only a genuine end of file returns an empty string: the DuckyScript
    // interpreter uses that to detect EOF.
    if (!found)
    {
        return "";
    }

    if (!ret.empty() && ret.back() == '\r')
    {
        ret.pop_back();
    }

    // A blank line inside the script must be skipped, not mistaken for EOF.
    // Return a single space so the parser trims it to empty and advances.
    if (ret.empty())
    {
        return " ";
    }

    return ret;
}

static void listDir(std::vector<std::string> &files, fs::FS &fs, const char *dirname)
{
    if (dirname == nullptr || dirname[0] == '\0') return;
    File root = fs.open(dirname, "r");
    if (!root)
    {
        return;
    }
    if (!root.isDirectory())
    {
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            const char* path =
            file.path();
            if (path != nullptr)
            {
                listDir(files, fs, path);
            }
        }
        else
        {
            // name() only returns the basename on arduino-esp32 3.x, which
            // would flatten files inside subdirectories onto "/". path()
            // returns the full path ("/sub/payload.ds").
            const char *path = file.path();
            if (path != nullptr && path[0] != '\0')
            {
                files.emplace_back(path[0] == '/' ? std::string(path) : std::string("/") + path);
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

std::vector<std::string> StorageDevice::listFiles()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running)
    {
        return {};
    }

    if (filesCache.size() != 0)
    {
        return filesCache;
    }

    listDir(filesCache, FILE_INTERFACE, "/");

    return filesCache;
}

bool StorageDevice::listDirectory(const std::string& path, std::vector<std::string>& directories, std::vector<std::string>& files,
                                  size_t maxEntries, bool& directoriesTruncated, bool& filesTruncated)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    directories.clear();
    files.clear();
    directoriesTruncated = false;
    filesTruncated = false;
    if (!running || !isSafeUserPath(path)) return false;

    File root = FILE_INTERFACE.open(path.c_str(), "r");
    if (!root) return false;
    if (!root.isDirectory())
    {
        root.close();
        return false;
    }

    File entry = root.openNextFile();
    while (entry)
    {
        const char *entryPath = entry.path();
        if (entryPath != nullptr && entryPath[0] != '\0')
        {
            std::string childPath = entryPath[0] == '/' ? entryPath : "/" + std::string(entryPath);
            if (isSafeUserPath(childPath))
            {
                if (entry.isDirectory())
                {
                    if (directories.size() < maxEntries) directories.emplace_back(childPath);
                    else directoriesTruncated = true;
                }
                else
                {
                    if (files.size() < maxEntries) files.emplace_back(childPath);
                    else filesTruncated = true;
                }
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
    return true;
}

bool StorageDevice::getUsage(uint64_t &used, uint64_t &total)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    used = 0;
    total = 0;
    if (!running) return false;

    total = FILE_INTERFACE.totalBytes();
    used = FILE_INTERFACE.usedBytes();
    return total != 0 && used <= total;
}

bool StorageDevice::createEmptyFile(const std::string &filename)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename)) return false;
    File file = FILE_INTERFACE.open(filename.c_str(), "w");
    if (!file)
    {
        return false;
    }
    file.close();
    refreshCache();
    return true;
}

bool StorageDevice::deleteFile(const std::string& filename)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || !isSafeUserPath(filename)) return false;
    const bool removed = FILE_INTERFACE.remove(filename.c_str());
    if (removed)
    {
        refreshCache();
    }
    return removed;
}

bool StorageDevice::moveFile(const std::string& source, const std::string& destination)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || source == destination || !isSafeUserPath(source) || !isSafeUserPath(destination) ||
        !FILE_INTERFACE.exists(source.c_str()) || FILE_INTERFACE.exists(destination.c_str())) return false;
    File file = FILE_INTERFACE.open(source.c_str(), "r");
    if (!file || file.isDirectory()) return false;
    file.close();
    const bool moved = FILE_INTERFACE.rename(source.c_str(), destination.c_str());
    if (moved) refreshCache();
    return moved;
}

bool StorageDevice::commitUpload(const std::string& temporary, const std::string& destination, bool replaceExisting)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    const std::string backup = temporary + ".bak";
    if (!running || temporary == destination || !isSafeUserPath(temporary) ||
        !isSafeUserPath(destination) || !isSafeUserPath(backup) || FILE_INTERFACE.exists(backup.c_str())) return false;
    File staged = FILE_INTERFACE.open(temporary.c_str(), "r");
    if (!staged || staged.isDirectory()) return false;
    staged.close();
    const bool existed = FILE_INTERFACE.exists(destination.c_str());
    if (existed)
    {
        File target = FILE_INTERFACE.open(destination.c_str(), "r");
        if (!target || target.isDirectory()) return false;
        target.close();
    }
    if (existed && (!replaceExisting || !FILE_INTERFACE.rename(destination.c_str(), backup.c_str()))) return false;
    // FAT cannot rename over an existing file. Keep the old file until the
    // complete upload is in place; preserve the backup if rollback also fails.
    // NOTE: not power-loss atomic; add a recovery journal if that is required.
    const bool committed = FILE_INTERFACE.rename(temporary.c_str(), destination.c_str());
    if (existed)
    {
        if (committed) FILE_INTERFACE.remove(backup.c_str());
        else FILE_INTERFACE.rename(backup.c_str(), destination.c_str());
    }
    refreshCache();
    return committed;
}

StorageDevice::StorageDevice()
{
#ifndef NO_SD
#if !defined(USE_SD_INTERFACE)
    SD_MMC = FILE_INTERFACE;
#endif
#endif
}

bool StorageDevice::isRawAccessSupported()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
#if defined(USE_SD_INTERFACE) or defined(USE_SD_MMC_INTERFACE)
    return true;
#else
    return false;
#endif
}

size_t StorageDevice::sectorSize()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running) return 0;
#if defined(USE_SD_INTERFACE)
    return FILE_INTERFACE.sectorSize();
#elif defined (USE_SD_MMC_INTERFACE)
    if (FILE_INTERFACE.getCard() == nullptr)
    {
        return 0;
    }
    return FILE_INTERFACE.getCard()->csd.sector_size;
#else
    return 0;
#endif
}

size_t StorageDevice::deviceCapacity()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running) return 0;
#if defined(USE_SD_INTERFACE)
    return FILE_INTERFACE.cardSize();
#elif defined (USE_SD_MMC_INTERFACE)
    auto *card = FILE_INTERFACE.getCard();
    return card != nullptr ? card->csd.capacity : 0;
#else
    return 0;
#endif
}

int32_t StorageDevice::readRawSectors(uint8_t* buffer, uint32_t lba, uint32_t sectors)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || buffer == nullptr || (reinterpret_cast<uintptr_t>(buffer) & 0x3U) != 0 ||
        sectors == 0 || sectors - 1 > UINT32_MAX - lba) return -1;
    const size_t bytesPerSector = sectorSize();
    if (bytesPerSector == 0 || sectors > (static_cast<uint32_t>(INT32_MAX) / bytesPerSector)) return -1;
#if defined(USE_SD_INTERFACE)
    for (uint32_t sector = 0; sector < sectors; ++sector)
    {
        if (!FILE_INTERFACE.readRAW(buffer + (sector * bytesPerSector), lba + sector)) return -1;
    }
    return static_cast<int32_t>(sectors * bytesPerSector);
#elif defined (USE_SD_MMC_INTERFACE)
    auto *card = FILE_INTERFACE.getCard();
    if (card != nullptr && sdmmc_read_sectors(card, buffer, lba, sectors) == ESP_OK)
    {
        return sectors * card->csd.sector_size;
    }
    return -1;
#else
    return -1;
#endif
}

int32_t StorageDevice::writeRawSectors(uint8_t* buffer, uint32_t lba, uint32_t sectors)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (!running || buffer == nullptr || (reinterpret_cast<uintptr_t>(buffer) & 0x3U) != 0 ||
        sectors == 0 || sectors - 1 > UINT32_MAX - lba) return -1;
    const size_t bytesPerSector = sectorSize();
    if (bytesPerSector == 0 || sectors > (static_cast<uint32_t>(INT32_MAX) / bytesPerSector)) return -1;
#if defined(USE_SD_INTERFACE)
    for (uint32_t sector = 0; sector < sectors; ++sector)
    {
        if (!FILE_INTERFACE.writeRAW(buffer + (sector * bytesPerSector), lba + sector)) return -1;
    }
    return static_cast<int32_t>(sectors * bytesPerSector);
#elif defined (USE_SD_MMC_INTERFACE)
    auto *card = FILE_INTERFACE.getCard();
    if (card != nullptr && sdmmc_write_sectors(card, buffer, lba, sectors) == ESP_OK)
    {
        return sectors * card->csd.sector_size;
    }
    return -1;
#else
    return -1;
#endif
}

void StorageDevice::flush()
{
    // This backend does not expose raw sector access.
}

void StorageDevice::refreshCache()
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    filesCache.clear();
}

void StorageDevice::loop(Preferences &prefs)
{
}

void StorageDevice::begin(Preferences &prefs)
{
    this->begin(prefs, false);
}

void StorageDevice::begin(Preferences &prefs, bool format)
{
    const std::lock_guard<std::recursive_mutex> lock(storageMutex);
    if (running)
    {
        FILE_INTERFACE.end();
    }
    running = false;
    refreshCache();
#ifdef NO_SD
    if (!FILE_INTERFACE.begin(format, "/sdcard"))
    {
        Debug::Log.info(LOG_MMC, "Storage interface could not start.");
    }
    else
    {
        running = true;
    }
#elif defined (SD_MMC_D1_PIN)
    FILE_INTERFACE.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);
    if (!FILE_INTERFACE.begin("/sdcard", false, format, SDMMC_FREQ_52M))
    {
        Debug::Log.info(LOG_MMC, "Storage interface could not start.");
    }
    else
    {
        uint8_t cardType = FILE_INTERFACE.cardType();
        if (cardType == CARD_NONE)
        {
            Debug::Log.info(LOG_MMC, "No SD card detected.");
        }
        else
        {
            running = true;
        }
    }
#elif defined(USE_SD_INTERFACE)
    pinMode(SD_MISO_PIN, INPUT_PULLUP);
    SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!FILE_INTERFACE.begin(SD_CS_PIN))
    {
        Debug::Log.info(LOG_MMC, "Storage interface could not start.");
    }
    else
    {
        running = true;
    }
#endif
    if (running)
    {
        ensureStandardDirectories();
        refreshCache();
    }
}
