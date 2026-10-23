// libFuzzer target for the stateless HTTP input validators. It feeds arbitrary
// bytes through every WebRequestValidation helper so malformed input that
// crashes or reads out of bounds is caught by AddressSanitizer.
//
// Build and run (Clang):
//   clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address \
//     -Ifuzz -Isrc/Comms/Web fuzz/validation_fuzz.cpp -o validation_fuzz
//   ./validation_fuzz -max_total_time=30
#include <cstddef>
#include <cstdint>
#include <string>

#include "RequestValidation.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  const std::string input(reinterpret_cast<const char *>(data), size);
  const String value(input);

  WebRequestValidation::validUsername(value);
  WebRequestValidation::validStoragePath(value);
  WebRequestValidation::validUploadFilename(value);
  WebRequestValidation::validCommand(value);
  WebRequestValidation::isMarauderCommand(value);
  WebRequestValidation::validWifiText(value, 64);
  (void)WebRequestValidation::hasExtensionIgnoreCase(value, ".ds");
  (void)WebRequestValidation::fileTypeForPath(value);
  return 0;
}
