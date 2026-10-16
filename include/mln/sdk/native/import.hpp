#pragma once

#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include "mln/sdk/native/buffer.hpp"
#include "mln/sdk/native/traits.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Mln::Sdk::Native {

    [[noreturn]] inline void invalid_import_buffer(const char *message) {
        std::fputs("MyeLiN native import: ", stderr);
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
        std::abort();
    }

    namespace Import {
        template <typename T> class Argument {
            public:
                using value_type = physical_type_t<T>;

                explicit Argument(T value) : value_(prepare(std::move(value))) {}
                Argument(const Argument &) = delete;
                Argument &operator=(const Argument &) = delete;

                Argument(Argument &&) noexcept = default;
                Argument &operator=(Argument &&) noexcept = default;
                ~Argument() = default;

                value_type value() const { return value_; }

            private:
                value_type prepare(T input) {
                    using A = clean_t<T>;
                    if constexpr (std::is_same_v<A, int32_t>) {
                        return std::bit_cast<uint32_t>(input);
                    } else if constexpr (std::is_same_v<A, int64_t>) {
                        return std::bit_cast<uint64_t>(input);
                    } else if constexpr (std::is_same_v<A, uint32_t> ||
                                         std::is_same_v<A, uint64_t> || std::is_same_v<A, float> ||
                                         std::is_same_v<A, double>) {
                        return input;
                    } else if constexpr (is_buffer_v<A>) {
                        if constexpr (std::is_lvalue_reference_v<T>)
                            return ::Mln::Sdk::Native::Argument<A>::identifier(input);
                        else
                            return Return<A>::write(std::move(input));
                    } else if constexpr (is_span_const_v<A>) {
                        using U = typename A::value_type;
                        static_assert(std::is_trivially_copyable_v<U> && !std::is_same_v<U, bool>,
                                      "IMPORT: std::span<const T> requires a trivially copyable, "
                                      "non-bool T because its bytes are copied into a Buffer");
                        if (input.size() > std::numeric_limits<uint32_t>::max() / sizeof(U))
                            invalid_import_buffer("span is too large for a 32-bit Buffer");
                        uint32_t bytes = static_cast<uint32_t>(input.size() * sizeof(U));
                        owned_buffer_ = ::Mln::Buffer(bytes);
                        if (bytes != 0)
                            owned_buffer_.write(std::as_bytes(input));
                        return ::Mln::Sdk::Native::Argument<::Mln::Buffer>::identifier(
                            owned_buffer_);
                    } else if constexpr (is_structured_value_v<A>) {
                        ::Mln::Sdk::Native::Return<void>::Writer writer;
                        writer.write(input);
                        if (writer.size() > std::numeric_limits<uint32_t>::max())
                            invalid_import_buffer("valor codificado excede o limite de 32 bits");
                        uint32_t bytes = static_cast<uint32_t>(writer.size());
                        owned_buffer_ = ::Mln::Buffer(bytes);
                        if (bytes != 0)
                            owned_buffer_.write(writer.bytes());
                        return ::Mln::Sdk::Native::Argument<::Mln::Buffer>::identifier(
                            owned_buffer_);
                    } else {
                        static_assert(is_valid_arg_v<A>,
                                      "IMPORT: unsupported argument; use a Wasm scalar, "
                                      "std::span<const T>, Mln::Buffer, or a type "
                                      "with Native::Argument<T> and Native::Return<T>");
                    }
                }

                ::Mln::Buffer owned_buffer_;
                value_type value_{};
        };
    }

    namespace Import {
        template <typename T> T read_return(physical_type_t<T> raw) {
            using R = clean_t<T>;
            if constexpr (std::is_same_v<R, int32_t>) {
                return std::bit_cast<int32_t>(raw);
            } else if constexpr (std::is_same_v<R, int64_t>) {
                return std::bit_cast<int64_t>(raw);
            } else if constexpr (std::is_same_v<R, uint32_t> || std::is_same_v<R, uint64_t> ||
                                 std::is_same_v<R, float> || std::is_same_v<R, double>) {
                return raw;
            } else if constexpr (is_buffer_v<R>) {
                return ::Mln::Sdk::Native::Argument<R>::read(raw);
            } else if constexpr (is_structured_value_v<R>) {
                ::Mln::Buffer buffer = ::Mln::Sdk::Native::Argument<::Mln::Buffer>::read(raw);
                const std::size_t bytes = buffer.size();
                std::vector<std::byte> encoded(bytes);
                if (bytes != 0)
                    buffer.read(encoded);
                ::Mln::Sdk::Native::Argument<void>::Reader reader(encoded);
                R result = reader.template read<R>();
                if (!reader.empty())
                    invalid_import_buffer("Return<T> did not consume the entire result Buffer");
                return result;
            } else {
                static_assert(is_valid_result_v<R>,
                              "IMPORT: unsupported return; use void, a Wasm scalar, "
                              "Mln::Buffer, or a type with Native::Argument<T> and "
                              "Native::Return<T>");
            }
        }

        template <typename Signature, typename Contract> struct Function;

        template <typename Ret, typename... Args, typename Contract>
        struct Function<Ret(Args...), Contract> {
                static_assert((is_valid_arg_v<Args> && ...),
                              "IMPORT: unsupported argument; use a Wasm scalar, "
                              "std::span<const T>, Mln::Buffer, or a type "
                              "with Native::Argument<T> and Native::Return<T>");
                static_assert(is_valid_result_v<Ret>,
                              "IMPORT: unsupported return; use void, a Wasm scalar, "
                              "Mln::Buffer, or a type with Native::Argument<T> and "
                              "Native::Return<T>");

                Ret operator()(Args... args) const {
                    std::tuple<Argument<Args>...> arguments{Argument<Args>(std::move(args))...};
                    return invoke(arguments, std::index_sequence_for<Args...>{});
                }

            private:
                template <std::size_t... I>
                static Ret invoke(std::tuple<Argument<Args>...> &arguments,
                                  std::index_sequence<I...>) {
                    if constexpr (std::is_void_v<Ret>) {
                        Contract::template invoke<void>(std::get<I>(arguments).value()...);
                    } else {
                        physical_type_t<Ret> raw = Contract::template invoke<physical_type_t<Ret>>(
                            std::get<I>(arguments).value()...);
                        return read_return<Ret>(raw);
                    }
                }
        };
    }

}

#if defined(__wasm__)
#define MYE_NATIVE_IMPORT_INVOKE(module, name)                                                     \
    template <typename Ret, typename... Args>                                                      \
    __attribute__((import_module(#module), import_name(#name))) static Ret invoke(Args...)
#else
#define MYE_NATIVE_IMPORT_INVOKE(module, name)                                                     \
    template <typename Ret, typename... Args> static Ret invoke(Args...) {                         \
        ::Mln::raise(::Mln::Sdk::Errors::Native::Import::Outside::Wasm, #module, #name);           \
    }
#endif

#define MYE_NATIVE_IMPORT_TAG(name) name##_mln_import_contract

#define IMPORT(module, name, ...)                                                                  \
    struct MYE_NATIVE_IMPORT_TAG(name) {                                                           \
            using signature = __VA_ARGS__;                                                         \
            static_assert(::Mln::Sdk::Native::is_valid_signature_v<signature>,                     \
                          "IMPORT: unsupported signature; use Wasm scalars, "                      \
                          "std::span<const T>, Mln::Buffer values, or types "                      \
                          "with Native::Argument<T> and Native::Return<T>");                       \
            MYE_NATIVE_IMPORT_INVOKE(module, name);                                                \
    };                                                                                             \
    inline constexpr ::Mln::Sdk::Native::Import::Function<                                         \
        typename MYE_NATIVE_IMPORT_TAG(name)::signature, MYE_NATIVE_IMPORT_TAG(name)>              \
        name {}
