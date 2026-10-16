#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace Mln {
    class Library {
        public:
            static Library open(std::string_view path);
            static Library open(std::string_view name, std::span<const std::uint8_t> bytes);

            Library(Library &&) noexcept = default;
            Library &operator=(Library &&) noexcept = default;
            Library(const Library &) = delete;
            Library &operator=(const Library &) = delete;
            ~Library() = default;

            [[nodiscard]] void *symbol(std::string_view name) const noexcept;
            [[nodiscard]] static std::string error();

        private:
            struct Descriptor {
                    Descriptor() = default;
#if defined(_WIN32)
                    explicit Descriptor(std::string path) noexcept;
#else
                    explicit Descriptor(int value) noexcept;
#endif
                    Descriptor(Descriptor &&other) noexcept;
                    Descriptor &operator=(Descriptor &&other) noexcept;
                    Descriptor(const Descriptor &) = delete;
                    Descriptor &operator=(const Descriptor &) = delete;
                    ~Descriptor();

#if defined(_WIN32)
                    std::string path;
#else
                    int value = -1;
#endif
            };

            using Handle = std::unique_ptr<void, void (*)(void *)>;

            Library(Descriptor descriptor, Handle handle) noexcept;

            Descriptor descriptor_;
            Handle handle_{nullptr, nullptr};
    };
} // namespace Mln
