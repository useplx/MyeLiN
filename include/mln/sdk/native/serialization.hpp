#pragma once

#include "mln/sdk/errors/sdk.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Mln::Sdk::Native {
    template <typename T, typename Enable = void> struct Argument;
    template <typename T, typename Enable = void> struct Return;

    template <> struct Return<void> {
            class Writer {
                public:
                    void write(std::span<const std::byte> bytes) {
                        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
                    }

                    template <typename T> void write(const T &value) {
                        Return<std::remove_cvref_t<T>>::write(*this, value);
                    }

                    template <typename T> void write_field(const T &value) {
                        Writer field;
                        field.write(value);
                        if (field.size() > std::numeric_limits<std::uint32_t>::max())
                            raise(Errors::Native::Serialization::Field::Size::Overflow,
                                  field.size());
                        write(static_cast<std::uint32_t>(field.size()));
                        write(field.bytes());
                    }

                    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
                        return bytes_;
                    }
                    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

                private:
                    std::vector<std::byte> bytes_;
            };
    };

    template <> struct Argument<void> {
            class Reader {
                public:
                    explicit Reader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

                    void read(std::span<std::byte> destination) {
                        require(destination.size());
                        if (!destination.empty())
                            std::memcpy(destination.data(), bytes_.data() + offset_,
                                        destination.size());
                        offset_ += destination.size();
                    }

                    template <typename T> [[nodiscard]] T read() {
                        return Argument<std::remove_cvref_t<T>>::read(*this);
                    }

                    template <typename T> [[nodiscard]] T read_field() {
                        const std::uint32_t size = read<std::uint32_t>();
                        require(size);
                        Reader field(bytes_.subspan(offset_, size));
                        T value = field.read<T>();
                        if (!field.empty())
                            raise(Errors::Native::Serialization::Field::Bytes::Trailing,
                                  field.remaining());
                        offset_ += size;
                        return value;
                    }

                    [[nodiscard]] std::size_t remaining() const noexcept {
                        return bytes_.size() - offset_;
                    }
                    [[nodiscard]] bool empty() const noexcept { return remaining() == 0; }

                private:
                    void require(std::size_t size) const {
                        if (size > remaining())
                            raise(Errors::Native::Serialization::Value::Truncated, size,
                                  remaining());
                    }

                    std::span<const std::byte> bytes_;
                    std::size_t offset_ = 0;
            };
    };

    template <typename T>
    struct Return<T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>> {
            static constexpr std::size_t fixed_size = sizeof(T);

            static void write(Return<void>::Writer &writer, T value) {
                static_assert(std::endian::native == std::endian::little);
                writer.write(std::as_bytes(std::span(&value, 1)));
            }
    };

    template <typename T>
    struct Argument<T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>> {
            static T read(Argument<void>::Reader &reader) {
                T value{};
                reader.read(std::as_writable_bytes(std::span(&value, 1)));
                return value;
            }
    };

    template <typename T> struct Return<T, std::enable_if_t<std::is_enum_v<T>>> {
            static constexpr std::size_t fixed_size = sizeof(std::underlying_type_t<T>);
            static void write(Return<void>::Writer &writer, T value) {
                writer.write(static_cast<std::underlying_type_t<T>>(value));
            }
    };

    template <typename T> struct Argument<T, std::enable_if_t<std::is_enum_v<T>>> {
            static T read(Argument<void>::Reader &reader) {
                return static_cast<T>(reader.read<std::underlying_type_t<T>>());
            }
    };

    template <> struct Return<std::string> {
            static void write(Return<void>::Writer &writer, const std::string &value) {
                writer.write(std::as_bytes(std::span(value)));
            }
    };

    template <> struct Argument<std::string> {
            static std::string read(Argument<void>::Reader &reader) {
                std::string value(reader.remaining(), '\0');
                reader.read(std::as_writable_bytes(std::span(value)));
                return value;
            }
    };

    template <typename T> inline constexpr bool fixed_return = requires { Return<T>::fixed_size; };

    template <typename T> struct Return<std::vector<T>> {
            static void write(Return<void>::Writer &writer, const std::vector<T> &values) {
                if constexpr (fixed_return<T>) {
                    for (const T &value : values)
                        writer.write(value);
                } else {
                    if (values.size() > std::numeric_limits<std::uint32_t>::max())
                        raise(Errors::Native::Serialization::Vector::Size::Overflow, values.size());
                    writer.write(static_cast<std::uint32_t>(values.size()));
                    for (const T &value : values)
                        writer.write_field(value);
                }
            }
    };

    template <typename T> struct Argument<std::vector<T>> {
            static std::vector<T> read(Argument<void>::Reader &reader) {
                std::vector<T> values;
                if constexpr (fixed_return<T>) {
                    constexpr std::size_t width = Return<T>::fixed_size;
                    if (reader.remaining() % width != 0)
                        raise(Errors::Native::Serialization::Vector::Storage::Size::Invalid,
                              reader.remaining(), width);
                    values.reserve(reader.remaining() / width);
                    while (!reader.empty())
                        values.push_back(reader.read<T>());
                } else {
                    const std::uint32_t count = reader.read<std::uint32_t>();
                    values.reserve(count);
                    for (std::uint32_t index = 0; index < count; ++index)
                        values.push_back(reader.read_field<T>());
                }
                return values;
            }
    };

    template <typename A, typename B> struct Return<std::pair<A, B>> {
            static void write(Return<void>::Writer &writer, const std::pair<A, B> &value) {
                writer.write_field(value.first);
                writer.write_field(value.second);
            }
    };

    template <typename A, typename B> struct Argument<std::pair<A, B>> {
            static std::pair<A, B> read(Argument<void>::Reader &reader) {
                return {reader.read_field<A>(), reader.read_field<B>()};
            }
    };

    template <typename... T> struct Return<std::tuple<T...>> {
            static void write(Return<void>::Writer &writer, const std::tuple<T...> &value) {
                std::apply([&](const auto &...fields) { (writer.write_field(fields), ...); },
                           value);
            }
    };

    template <typename... T> struct Argument<std::tuple<T...>> {
            static std::tuple<T...> read(Argument<void>::Reader &reader) {
                return {reader.read_field<T>()...};
            }
    };

    template <typename T, std::size_t Size> struct Return<std::array<T, Size>> {
            static void write(Return<void>::Writer &writer, const std::array<T, Size> &value) {
                for (const T &element : value)
                    writer.write_field(element);
            }
    };

    template <typename T, std::size_t Size> struct Argument<std::array<T, Size>> {
            static std::array<T, Size> read(Argument<void>::Reader &reader) {
                std::array<T, Size> value{};
                for (T &element : value)
                    element = reader.read_field<T>();
                return value;
            }
    };
}
