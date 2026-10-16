#include "library.hpp"

#include "mln/error.hpp"
#include "mln/runtime/errors.hpp"

#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>
#include <filesystem>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#if defined(__linux__)
#include <linux/memfd.h>
#endif
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include <utility>

namespace Mln {
    namespace {
        void close_library(void *library) {
            if (!library) return;
#if defined(_WIN32)
            FreeLibrary(static_cast<HMODULE>(library));
#else
                dlclose(library);
#endif
        }
    } // namespace

#if defined(_WIN32)
    Library::Descriptor::Descriptor(std::string path_value) noexcept
        : path(std::move(path_value)) {}
#else
    Library::Descriptor::Descriptor(int descriptor) noexcept : value(descriptor) {}
#endif

    Library::Descriptor::Descriptor(Descriptor &&other) noexcept
#if defined(_WIN32)
        : path(std::move(other.path)) {}
#else
        : value(std::exchange(other.value, -1)) {}
#endif

    Library::Descriptor &Library::Descriptor::operator=(Descriptor &&other) noexcept {
        if (this != &other) {
#if defined(_WIN32)
            if (!path.empty()) std::filesystem::remove(path);
            path = std::move(other.path);
#else
            if (value >= 0)
                close(value);
            value = std::exchange(other.value, -1);
#endif
        }
        return *this;
    }

    Library::Descriptor::~Descriptor() {
#if defined(_WIN32)
        if (!path.empty()) std::filesystem::remove(path);
#else
        if (value >= 0)
            close(value);
#endif
    }

    Library::Library(Descriptor descriptor, Handle handle) noexcept
        : descriptor_(std::move(descriptor)), handle_(std::move(handle)) {}

    Library Library::open(std::string_view path) {
        const std::string path_string(path);
#if defined(_WIN32)
        Handle handle(LoadLibraryA(path_string.c_str()), close_library);
#else
        Handle handle(dlopen(path_string.c_str(), RTLD_NOW), close_library);
#endif
        if (!handle)
            raise(Runtime::Errors::Native::Library::Load::Failed, path, error());
        return Library(Descriptor{}, std::move(handle));
    }

    Library Library::open(std::string_view name, std::span<const std::uint8_t> bytes) {
#if defined(_WIN32)
        char directory[MAX_PATH + 1]{};
        if (GetTempPathA(MAX_PATH, directory) == 0)
            raise(Runtime::Errors::Native::Library::Materialize::Failed, name, error());
        char path[MAX_PATH + 1]{};
        if (GetTempFileNameA(directory, "mln", 0, path) == 0)
            raise(Runtime::Errors::Native::Library::Materialize::Failed, name, error());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) {
            std::filesystem::remove(path);
            raise(Runtime::Errors::Native::Library::Materialize::Failed, name,
                  "failed to write temporary DLL");
        }
        Descriptor descriptor(path);
        Handle handle(LoadLibraryA(path), close_library);
        if (!handle)
            raise(Runtime::Errors::Native::Library::Load::Failed, name, error());
        return Library(std::move(descriptor), std::move(handle));
#elif defined(__linux__)
        std::string fd_name = "myelin-";
        fd_name.append(name);
        const int raw_fd =
            static_cast<int>(syscall(SYS_memfd_create, fd_name.c_str(), MFD_CLOEXEC));
        if (raw_fd < 0)
            raise(Runtime::Errors::Native::Library::Memfd::Failed, name, std::strerror(errno));
        Descriptor descriptor(raw_fd);
        std::size_t written = 0;
        while (written < bytes.size()) {
            const ssize_t count =
                ::write(descriptor.value, bytes.data() + written, bytes.size() - written);
            if (count < 0)
                raise(Runtime::Errors::Native::Library::Materialize::Failed, name,
                      std::strerror(errno));
            if (count == 0)
                raise(Runtime::Errors::Native::Library::Write::Incomplete, name);
            written += static_cast<std::size_t>(count);
        }
        const std::string path = std::format("/proc/self/fd/{}", descriptor.value);
        Handle handle(dlopen(path.c_str(), RTLD_NOW), close_library);
        if (!handle)
            raise(Runtime::Errors::Native::Library::Load::Failed, name, error());
        return Library(std::move(descriptor), std::move(handle));
#else
        (void)name;
        (void)bytes;
        raise(Runtime::Errors::Native::Library::Memfd::Unsupported);
#endif
    }

    void *Library::symbol(std::string_view name) const noexcept {
        const std::string symbol_name(name);
#if defined(_WIN32)
        return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(handle_.get()),
                                                        symbol_name.c_str()));
#else
        dlerror();
        return dlsym(handle_.get(), symbol_name.c_str());
#endif
    }

    std::string Library::error() {
#if defined(_WIN32)
        const DWORD code = GetLastError();
        char *message = nullptr;
        const DWORD length = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code, 0, reinterpret_cast<char *>(&message), 0, nullptr);
        std::string result = length && message ? std::string(message, length)
                                               : std::format("Windows error {}", code);
        if (message) LocalFree(message);
        return result;
#else
        const char *message = dlerror();
        return message ? message : "unknown dynamic library error";
#endif
    }
} // namespace Mln
