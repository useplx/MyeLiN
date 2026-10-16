#include "mln/package.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    // Keep individual fuzz cases bounded; large valid packages are exercised by
    // integration tests while this target focuses on parser safety.
    if (size > 64 * 1024 * 1024)
        return 0;

    char path[] = "/tmp/myelin-package-fuzz-XXXXXX";
    const int descriptor = ::mkstemp(path);
    if (descriptor < 0)
        return 0;
    struct Cleanup {
            int descriptor;
            const char *path;
            ~Cleanup() {
                ::close(descriptor);
                ::unlink(path);
            }
    } cleanup{descriptor, path};

    std::size_t written = 0;
    while (written < size) {
        const ssize_t count = ::write(descriptor, data + written, size - written);
        if (count <= 0)
            return 0;
        written += static_cast<std::size_t>(count);
    }

    try {
        (void)Mln::Package::File::open(path);
    } catch (...) {
        // Invalid inputs are expected. Sanitizers and libFuzzer detect crashes,
        // undefined behavior, leaks, and pathological resource consumption.
    }
    return 0;
}
