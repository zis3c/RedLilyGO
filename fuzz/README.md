# Fuzz targets

Coverage-guided fuzzing for parser and validation code that must handle
arbitrary input. Targets are built with Clang's libFuzzer and AddressSanitizer.

## validation_fuzz

Feeds arbitrary bytes through every `WebRequestValidation` helper (username,
storage path, upload filename, command, Marauder command, Wi-Fi text, extension
and file type). The `Arduino.h` shim provides the small `String` subset the
header uses, so no Arduino toolchain is needed.

Build and run:

```bash
clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address \
  -Ifuzz -Isrc/Comms/Web fuzz/validation_fuzz.cpp -o validation_fuzz
./validation_fuzz -max_total_time=30 -rss_limit_mb=0
```

The CI job runs this for 30 seconds on every push and pull request. A crash,
out-of-bounds read, or sanitizer report fails the job.

## csrf_fuzz

Feeds arbitrary bytes as an `X-CSRF-Token` header and asserts the acceptance
invariant: a token is accepted only when it is non-empty, exactly 16 lowercase
hex characters, and byte-equal to the current boot token. This guards the
comparison against length/case confusion and out-of-bounds reads even though the
format is fixed.

Build and run:

```bash
clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address \
  -Ifuzz fuzz/csrf_fuzz.cpp -o csrf_fuzz
./csrf_fuzz -max_total_time=30 -rss_limit_mb=0
```

The CI job also runs this for 30 seconds; both targets must stay clean.
