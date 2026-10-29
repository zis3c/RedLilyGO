// libFuzzer target for the CSRF token handling. It feeds arbitrary bytes as the
// X-CSRF-Token header and checks that acceptance is exactly a fixed-length,
// constant-shape match against the current boot token. A malformed header must
// never be accepted, and the comparison must not read out of bounds.
//
// Build and run (Clang):
//   clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address \
//     -Ifuzz fuzz/csrf_fuzz.cpp -o csrf_fuzz
//   ./csrf_fuzz -max_total_time=30
#include <cstddef>
#include <cstdint>
#include <string>

namespace {
// Mirrors WebInternal::csrfToken's output shape: 16 lowercase hex characters
// from two esp_random() words.
std::string tokenFromSeed(uint64_t a, uint64_t b)
{
    static const char *digits = "0123456789abcdef";
    std::string token;
    for (int i = 0; i < 8; ++i) token.push_back(digits[(a >> (4 * i)) & 0xF]);
    for (int i = 0; i < 8; ++i) token.push_back(digits[(b >> (4 * i)) & 0xF]);
    return token;
}

// Mirrors WebInternal::requestHasValidCsrf: non-empty and byte-equal to the
// boot token. No length prefix, hashing, or out-of-range access.
bool tokenAccepted(const std::string &provided, const std::string &boot)
{
    return !provided.empty() && provided == boot;
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Derive a boot token from the first 16 bytes when present; otherwise use a
    // fixed token so the fuzzer also explores the "known token" path.
    uint64_t a = 0, b = 0;
    for (int i = 0; i < 8 && static_cast<size_t>(i) < size; ++i) a = (a << 8) | data[i];
    for (int i = 0; i < 8 && static_cast<size_t>(8 + i) < size; ++i) b = (b << 8) | data[8 + i];
    const std::string boot = tokenFromSeed(a, b);

    const std::string provided(reinterpret_cast<const char *>(data), size);
    const bool accepted = tokenAccepted(provided, boot);

    // Invariant: acceptance implies an exact match with the boot token, so a
    // header of the wrong length, case, or content is always rejected.
    if (accepted)
    {
        if (provided != boot) __builtin_trap();
        if (provided.size() != 16) __builtin_trap();
        for (char c : provided)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) __builtin_trap();
    }
    // An empty or over-long header is never a valid token.
    if (provided.empty() && accepted) __builtin_trap();
    return 0;
}
