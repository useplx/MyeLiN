#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Mln {
    class Runtime;

    namespace Sdk::Native {
        struct Context;
        template <typename T, typename Enable> struct Argument;
        template <typename T, typename Enable> struct Return;
    }

    class Buffer {
        public:
            struct Errors;

            Buffer() noexcept = default;
            explicit Buffer(std::size_t size);
            Buffer(std::size_t size, std::vector<std::int64_t> shape);

            Buffer(const Buffer &) = delete;
            Buffer &operator=(const Buffer &) = delete;
            Buffer(Buffer &&) noexcept = default;
            Buffer &operator=(Buffer &&) noexcept = default;
            ~Buffer() = default;

            [[nodiscard]] std::size_t size() const;
            [[nodiscard]] bool empty() const;
            [[nodiscard]] std::span<const std::int64_t> shape() const noexcept;

            void read(std::span<std::byte> destination) const;
            void read(std::size_t offset, std::span<std::byte> destination) const;
            void write(std::span<const std::byte> source);
            void write(std::size_t offset, std::span<const std::byte> source);

            [[nodiscard]] std::uint8_t byte_at(std::size_t index) const;
            void write_byte(std::size_t index, std::uint8_t value);

            [[nodiscard]] Buffer clone() const;

        private:
            friend class Runtime;
            template <typename T, typename Enable> friend struct Sdk::Native::Argument;
            template <typename T, typename Enable> friend struct Sdk::Native::Return;
            class Storage;

            explicit Buffer(std::shared_ptr<Storage> storage) noexcept;
            [[nodiscard]] static Buffer from_abi(std::uint32_t identifier, bool owning);
            [[nodiscard]] static Buffer from_native(Sdk::Native::Context &context,
                                                    std::uint32_t identifier);
            [[nodiscard]] static Buffer from_native(Sdk::Native::Context &context,
                                                    std::uint32_t identifier,
                                                    std::weak_ptr<const void> lifetime);
            [[nodiscard]] static Buffer from_native(Sdk::Native::Context &context,
                                                    std::uint32_t identifier,
                                                    std::weak_ptr<const void> lifetime,
                                                    std::vector<std::int64_t> shape);
            [[nodiscard]] std::uint32_t abi_id() const;
            [[nodiscard]] std::uint32_t release_to_abi();
            [[nodiscard]] std::uint32_t
            native_id(const Sdk::Native::Context &context) const noexcept;
            [[nodiscard]] const std::byte *host_data() const noexcept;
            [[nodiscard]] std::shared_ptr<const void> lifetime() const noexcept;

            std::shared_ptr<Storage> storage_;
    };
}
