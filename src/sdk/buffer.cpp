#include "mln/buffer.hpp"
#include "mln/buffer_errors.hpp"
#include "mln/sdk/native/abi.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#if defined(__wasm__)
#include "mln/abi/abi.hpp"
#endif

namespace Mln {
    class Buffer::Storage {
        public:
#if defined(__wasm__)
            class Identifier {
                public:
                    explicit Identifier(std::uint32_t value) noexcept : value_(value) {}
                    Identifier(const Identifier &) = delete;
                    Identifier &operator=(const Identifier &) = delete;
                    Identifier(Identifier &&other) noexcept
                        : value_(other.value_), owning_(std::exchange(other.owning_, false)) {}
                    Identifier &operator=(Identifier &&other) noexcept {
                        if (this != &other) {
                            reset();
                            value_ = other.value_;
                            owning_ = std::exchange(other.owning_, false);
                        }
                        return *this;
                    }
                    ~Identifier() { reset(); }

                    [[nodiscard]] std::uint32_t get() const noexcept { return value_; }
                    [[nodiscard]] std::uint32_t release() noexcept {
                        owning_ = false;
                        return value_;
                    }

                private:
                    void reset() noexcept {
                        if (owning_)
                            Sdk::Abi::Buffer::free(value_);
                        owning_ = false;
                    }

                    std::uint32_t value_;
                    bool owning_ = true;
            };

            explicit Storage(std::size_t size)
                : identifier_(Sdk::Abi::Buffer::allocate(checked_size(size))) {}

            Storage(std::size_t size, std::vector<std::int64_t>) : Storage(size) {}

            Storage(std::uint32_t identifier, bool owning) : borrowed_(owning ? 0 : identifier) {
                if (owning)
                    identifier_.emplace(identifier);
            }

            [[nodiscard]] std::size_t size() const {
                return Sdk::Abi::Buffer::length(identifier());
            }

            void read(std::size_t offset, std::span<std::byte> destination) const {
                const auto checked_offset = checked_size(offset);
                const auto checked_length = checked_size(destination.size());
                Sdk::Abi::Buffer::read(identifier(), checked_offset,
                                       static_cast<Sdk::Abi::ptr_t>(
                                           reinterpret_cast<std::uintptr_t>(destination.data())),
                                       checked_length);
            }

            void write(std::size_t offset, std::span<const std::byte> source) {
                const auto checked_offset = checked_size(offset);
                const auto checked_length = checked_size(source.size());
                Sdk::Abi::Buffer::write(identifier(), checked_offset,
                                        static_cast<Sdk::Abi::const_ptr_t>(
                                            reinterpret_cast<std::uintptr_t>(source.data())),
                                        checked_length);
            }

            [[nodiscard]] std::uint32_t identifier() const {
                return identifier_ ? identifier_->get() : borrowed_;
            }

            [[nodiscard]] std::uint32_t release() {
                if (!identifier_)
                    return borrowed_;
                return identifier_->release();
            }

            [[nodiscard]] const std::byte *data() const noexcept { return nullptr; }

        private:
            static std::uint32_t checked_size(std::size_t size) {
                if (size > std::numeric_limits<std::uint32_t>::max())
                    raise(Buffer::Errors::Abi::Size::Overflow, size);
                return static_cast<std::uint32_t>(size);
            }

            std::optional<Identifier> identifier_;
            std::uint32_t borrowed_ = 0;
#else
            explicit Storage(std::size_t size, std::vector<std::int64_t> shape = {})
                : bytes_(size), shape_(std::move(shape)) {}

            Storage(Sdk::Native::Context &context, std::uint32_t identifier)
                : context_(&context), identifier_(identifier) {}

            Storage(Sdk::Native::Context &context, std::uint32_t identifier,
                    std::weak_ptr<const void> lifetime, std::vector<std::int64_t> shape = {})
                : context_(&context), identifier_(identifier), lifetime_(std::move(lifetime)),
                  shape_(std::move(shape)) {}

            [[nodiscard]] std::size_t size() const {
                require_context();
                return context_ ? context_->buffer.length(context_->user_data, identifier_)
                                : bytes_.size();
            }

            void read(std::size_t offset, std::span<std::byte> destination) const {
                if (context_) {
                    require_context();
                    context_->buffer.read(context_->user_data, identifier_, checked(offset),
                                          destination.data(), checked(destination.size()));
                    return;
                }
                if (destination.size() > bytes_.size() ||
                    offset > bytes_.size() - destination.size())
                    raise(Buffer::Errors::Read::Bounds::Exceeded, offset, destination.size(),
                          bytes_.size());
                std::ranges::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
                                    destination.size(), destination.begin());
            }

            void write(std::size_t offset, std::span<const std::byte> source) {
                if (context_) {
                    require_context();
                    context_->buffer.write(context_->user_data, identifier_, checked(offset),
                                           source.data(), checked(source.size()));
                    return;
                }
                if (source.size() > bytes_.size() || offset > bytes_.size() - source.size())
                    raise(Buffer::Errors::Write::Bounds::Exceeded, offset, source.size(),
                          bytes_.size());
                std::ranges::copy_n(source.begin(), source.size(),
                                    bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
            }

            [[nodiscard]] const std::byte *data() const noexcept {
                if (lifetime_ && lifetime_->expired())
                    return nullptr;
                if (!context_ || !context_->buffer.data)
                    return bytes_.data();
                return static_cast<const std::byte *>(
                    context_->buffer.data(context_->user_data, identifier_));
            }

            [[nodiscard]] std::span<const std::int64_t> shape() const noexcept { return shape_; }

            [[nodiscard]] std::uint32_t
            native_id(const Sdk::Native::Context &context) const noexcept {
                if (lifetime_ && lifetime_->expired())
                    return 0;
                return context_ == &context ? identifier_ : 0;
            }

        private:
            void require_context() const {
                if (lifetime_ && lifetime_->expired())
                    raise(Buffer::Errors::Runtime::Lifetime::Ended);
            }

            static std::uint32_t checked(std::size_t size) {
                if (size > std::numeric_limits<std::uint32_t>::max())
                    raise(Buffer::Errors::Abi::Size::Overflow, size);
                return static_cast<std::uint32_t>(size);
            }

            std::vector<std::byte> bytes_;
            Sdk::Native::Context *context_ = nullptr;
            std::uint32_t identifier_ = 0;
            std::optional<std::weak_ptr<const void>> lifetime_;
            std::vector<std::int64_t> shape_;
#endif
    };

    Buffer::Buffer(std::size_t size) : storage_(std::make_shared<Storage>(size)) {}

    Buffer::Buffer(std::size_t size, std::vector<std::int64_t> shape)
        : storage_(std::make_shared<Storage>(size, std::move(shape))) {}

    Buffer::Buffer(std::shared_ptr<Storage> storage) noexcept : storage_(std::move(storage)) {}

    Buffer Buffer::from_abi(std::uint32_t identifier, bool owning) {
#if defined(__wasm__)
        return Buffer(std::make_shared<Storage>(identifier, owning));
#else
        (void)identifier;
        (void)owning;
        raise(Buffer::Errors::Abi::Client::Required);
#endif
    }

    Buffer Buffer::from_native(Sdk::Native::Context &context, std::uint32_t identifier) {
#if defined(__wasm__)
        (void)context;
        (void)identifier;
        raise(Buffer::Errors::Native::Inside::Wasm::Unavailable);
#else
        return Buffer(std::make_shared<Storage>(context, identifier));
#endif
    }

    Buffer Buffer::from_native(Sdk::Native::Context &context, std::uint32_t identifier,
                               std::weak_ptr<const void> lifetime) {
#if defined(__wasm__)
        (void)context;
        (void)identifier;
        (void)lifetime;
        raise(Buffer::Errors::Native::Inside::Wasm::Unavailable);
#else
        return Buffer(std::make_shared<Storage>(context, identifier, std::move(lifetime)));
#endif
    }

    Buffer Buffer::from_native(Sdk::Native::Context &context, std::uint32_t identifier,
                               std::weak_ptr<const void> lifetime,
                               std::vector<std::int64_t> shape) {
#if defined(__wasm__)
        (void)context;
        (void)identifier;
        (void)lifetime;
        (void)shape;
        raise(Buffer::Errors::Native::Inside::Wasm::Unavailable);
#else
        return Buffer(
            std::make_shared<Storage>(context, identifier, std::move(lifetime), std::move(shape)));
#endif
    }

    std::uint32_t Buffer::abi_id() const {
#if defined(__wasm__)
        if (!storage_)
            return 0;
        return storage_->identifier();
#else
        raise(Buffer::Errors::Abi::Identifier::Unavailable);
#endif
    }

    std::uint32_t Buffer::release_to_abi() {
#if defined(__wasm__)
        if (!storage_)
            return 0;
        const std::uint32_t identifier = storage_->release();
        storage_.reset();
        return identifier;
#else
        raise(Buffer::Errors::Abi::Ownership::Transfer::Unavailable);
#endif
    }

    std::uint32_t Buffer::native_id(const Sdk::Native::Context &context) const noexcept {
#if defined(__wasm__)
        (void)context;
        return 0;
#else
        return storage_ ? storage_->native_id(context) : 0;
#endif
    }

    const std::byte *Buffer::host_data() const noexcept {
        return storage_ ? storage_->data() : nullptr;
    }

    std::shared_ptr<const void> Buffer::lifetime() const noexcept { return storage_; }

    std::size_t Buffer::size() const { return storage_ ? storage_->size() : 0; }

    bool Buffer::empty() const { return size() == 0; }

    std::span<const std::int64_t> Buffer::shape() const noexcept {
#if defined(__wasm__)
        return {};
#else
        return storage_ ? storage_->shape() : std::span<const std::int64_t>{};
#endif
    }

    void Buffer::read(std::span<std::byte> destination) const { read(0, destination); }

    void Buffer::read(std::size_t offset, std::span<std::byte> destination) const {
        if (!storage_)
            raise(Buffer::Errors::Read::Storage::Missing);
        storage_->read(offset, destination);
    }

    void Buffer::write(std::span<const std::byte> source) { write(0, source); }

    void Buffer::write(std::size_t offset, std::span<const std::byte> source) {
        if (!storage_)
            raise(Buffer::Errors::Write::Storage::Missing);
        storage_->write(offset, source);
    }

    std::uint8_t Buffer::byte_at(std::size_t index) const {
        std::uint8_t value = 0;
        read(index, std::as_writable_bytes(std::span(&value, 1)));
        return value;
    }

    void Buffer::write_byte(std::size_t index, std::uint8_t value) {
        write(index, std::as_bytes(std::span(&value, 1)));
    }

    Buffer Buffer::clone() const {
        const std::size_t byte_count = size();
        Buffer copy(byte_count, std::vector<std::int64_t>(shape().begin(), shape().end()));
        std::vector<std::byte> bytes(byte_count);
        read(bytes);
        copy.write(bytes);
        return copy;
    }
}
