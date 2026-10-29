/** @file SDInterface.cpp @brief SD card file and capture interface. */

#include "SDInterface.h"
#include "lang_var.h"
#include "../../../src/Devices/Storage/StorageDevice.h"


/** Initialize the configured SD or filesystem backend and cache card metadata. */
bool SDInterface::initSD() {
  return false;
}

/** Open a path through the selected filesystem abstraction. */
File SDInterface::getFile(String path) {
  const std::lock_guard<std::recursive_mutex> lock(Devices::Storage.lifecycleMutex());
  return SD.open(path, FILE_READ);
}

/** Remove a file through the selected filesystem abstraction. */
bool SDInterface::removeFile(String file_path) {
  const std::lock_guard<std::recursive_mutex> lock(Devices::Storage.lifecycleMutex());
  if (SD.remove(file_path))
    return true;
  else
    return false;
}

/** Recursively collect files matching an extension into the caller list. */
void SDInterface::listDirToLinkedList(LinkedList<String>* file_names, String str_dir, String ext) {
  const std::lock_guard<std::recursive_mutex> lock(Devices::Storage.lifecycleMutex());
  if (this->supported && file_names != nullptr) {
    File dir = SD.open(str_dir);
    if (!dir) return;
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry)
      {
        break;
      }

      if (entry.isDirectory()) {
        entry.close();
        continue;
      }

      String file_name = entry.name();
      if (ext != "") {
        if (file_name.endsWith(ext)) {
          file_names->add(file_name);
        }
      }
      else
        file_names->add(file_name);
    }
    dir.close();
  }
}

/** Print directory entries and their sizes for the command-line UI. */
void SDInterface::listDir(String str_dir){
  const std::lock_guard<std::recursive_mutex> lock(Devices::Storage.lifecycleMutex());
  if (this->supported) {
    File dir = SD.open(str_dir);
    if (!dir) return;
    while (true)
    {
      File entry = dir.openNextFile();
      if (! entry)
      {
        break;
      }
      //for (uint8_t i = 0; i < numTabs; i++)
      //{
      //  esp32m_print('\t');
      //}
      esp32m_print(entry.name());
      esp32m_print("\t");
      esp32m_println(entry.size());
      entry.close();
    }
    dir.close();
  }
}

/** Read the optional card-detect GPIO, if the board defines one. */
bool SDInterface::checkDetectPin() {
  #ifdef KIT
    if (digitalRead(SD_DET) == LOW)
      return true;
    else
      return false;
  #endif

  return false;
}

/** Service card presence and storage state for the firmware loop. */
void SDInterface::main() {
  if (!this->supported) {
    if (checkDetectPin()) {
      delay(100);
      this->initSD();
    }
  }
}
