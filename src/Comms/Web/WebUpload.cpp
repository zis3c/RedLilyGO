/** @file WebUpload.cpp @brief Authenticated multipart file uploads to SD storage. */

#ifndef NO_WEB
#include "WebInternal.h"

#include <cstdio>
#include <esp_system.h>

#include "../../Devices/Storage/StorageDevice.h"
#include "../../Attacks/Ducky/DuckyPayload.h"
#include "Diagnostics/Logging.h"

#define LOG_WEB "WEB"

using namespace WebRequestValidation;

static constexpr size_t MAX_UPLOAD_BYTES = 4 * 1024 * 1024;

void WebInternal::handleUpload(AsyncWebServerRequest *request, String filename, size_t index,
                               uint8_t *data, size_t len, bool final)
{
  const std::lock_guard<std::recursive_mutex> lock(webLifecycleMutex);
  const auto &url = request->url();

  if (url == "/uploadFile")
  {
    // A rejected upload may still deliver more chunks. Its response is final.
    if (request->getResponse() != nullptr) return;
    // This handler runs while the multipart body is still being parsed,
    // before the authentication gate in webRequestHandler gets a chance to
    // execute, so we must enforce credentials here ourselves. Reject and
    // remove any partially written file.
    if (rejectCrossSiteMutation(request)) return;
    if (!requestHasValidCsrf(request))
    {
      if (request->_tempFile) request->_tempFile.close();
      request->send(403, "application/json", "{\"error\":\"csrf_required\"}");
      return;
    }
    if (!requestAuthorized(request))
    {
      if (request->_tempFile) request->_tempFile.close();
      request->requestAuthentication();
      return;
    }
    const String requestedDirectory = request->hasParam("directory")
      ? request->getParam("directory")->value() : "Auto";
    const bool replaceExisting = request->hasParam("replace") && request->getParam("replace")->value() == "1";
    const std::string resolvedPath = Devices::Storage.resolveUploadPath(
      filename.c_str(), requestedDirectory.c_str());
    String storagePath = resolvedPath.c_str();
    // IDE saves can replace an existing file in a legacy/root/subfolder path.
    if (replaceExisting && request->hasParam("path"))
    {
      storagePath = request->getParam("path")->value();
      if (!validStoragePath(storagePath) ||
          storagePath.substring(storagePath.lastIndexOf('/') + 1) != filename)
      {
        request->send(400, "application/json", "{\"error\":\"invalid_save_path\"}");
        return;
      }
      if (!Devices::Storage.doesFileExist(storagePath.c_str()))
      {
        request->send(404, "application/json", "{\"error\":\"file_not_found\"}");
        return;
      }
    }
    if (!Devices::Storage.isRunning())
    {
      if (request->_tempFile) request->_tempFile.close();
      request->send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
      return;
    }
    if (index > MAX_UPLOAD_BYTES || len > MAX_UPLOAD_BYTES - index)
    {
      if (request->_tempFile) request->_tempFile.close();
      request->send(413, "application/json", "{\"error\":\"upload_too_large\"}");
      return;
    }
    // Upload names come from the multipart request. Keep them inside storage.
    if (!validUploadFilename(filename))
    {
      if (request->_tempFile) request->_tempFile.close();
      request->send(400, "text/plain", "Invalid filename");
      return;
    }
    if (storagePath.isEmpty())
    {
      if (request->_tempFile) request->_tempFile.close();
      request->send(400, "application/json", "{\"error\":\"invalid_save_folder\"}");
      return;
    }

    if (!index) {
      Devices::Storage.ensureStandardDirectories();
      if (!replaceExisting && Devices::Storage.doesFileExist(storagePath.c_str()))
      {
        if (request->_tempFile) request->_tempFile.close();
        request->send(409, "application/json", "{\"error\":\"file_exists\"}");
        return;
      }

      char temporary[48];
      snprintf(temporary, sizeof(temporary), "/Other/.upload-%08lx-%08lx.tmp",
               static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
      if (Devices::Storage.doesFileExist(temporary))
      {
        request->send(503, "application/json", "{\"error\":\"upload_busy\"}");
        return;
      }
      const String temporaryPath(temporary);
      request->_tempFile = Devices::Storage.openFile(temporary, "w");
      if (!request->_tempFile)
      {
        Debug::Log.error(LOG_WEB, std::string("File not available, no write permission"));
        request->send(500);
        return;
      }
      request->onDisconnect([request, temporaryPath]()
      {
        const std::lock_guard<std::recursive_mutex> storageLock(Devices::Storage.lifecycleMutex());
        if (request->_tempFile) request->_tempFile.close();
        Devices::Storage.deleteFile(temporaryPath.c_str());
      });
    }

    std::unique_lock<std::recursive_mutex> storageLock(Devices::Storage.lifecycleMutex());
    if (!request->_tempFile || request->_tempFile.position() != index)
    {
      request->send(400, "application/json", "{\"error\":\"invalid_upload_offset\"}");
      return;
    }
    if (len != 0)
    {
      if (request->_tempFile.write(data, len) != len)
      {
        Debug::Log.error(LOG_WEB, std::string("Error writing data to file"));
        request->_tempFile.close();
        request->send(500);
        return;
      }
    }
    else if (!final) // len == 0
    {
      Debug::Log.error(LOG_WEB, std::string("File data to write is 0 bytes"));
      request->_tempFile.close();
      request->send(500);
      return;
    }

    if (final)
    {
      const String temporaryPath = request->_tempFile.path();
      request->_tempFile.close();
      // Payload execution acquires payloadMutex before storage. Stop it with
      // storage unlocked, then serialize the existence check and rename.
      storageLock.unlock();
      if (hasExtensionIgnoreCase(filename, ".ds")) Attacks::Ducky.setPayload("");
      storageLock.lock();
      if (!replaceExisting && Devices::Storage.doesFileExist(storagePath.c_str()))
      {
        request->send(409, "application/json", "{\"error\":\"file_exists\"}");
        return;
      }
      if (!Devices::Storage.commitUpload(temporaryPath.c_str(), storagePath.c_str(), replaceExisting))
      {
        request->send(500, "application/json", "{\"error\":\"upload_commit_failed\"}");
        return;
      }
      Debug::Log.info(LOG_WEB, "File uploaded from web UI");
      Devices::Storage.refreshCache();
      request->send(200, "application/json", String("{\"ok\":true,\"path\":\"") + storagePath + "\"}");
    }
  }
}

#endif
