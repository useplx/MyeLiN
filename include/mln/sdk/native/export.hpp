#pragma once

#include "mln/sdk/native/abi.hpp"
#include "mln/sdk/native/buffer.hpp"
#include "mln/sdk/native/traits.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Mln::Sdk::Native::Export {

    using Native::Module;
    using Native::Symbol;

    [[noreturn]] inline void invalid_native_call(const char *message) {
        std::fputs("MyeLiN native symbol: ", stderr);
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
        std::abort();
    }

    class Provider {
        public:
            static Provider &instance() {
                static Provider provider;
                return provider;
            }

            void add(const char *module, const Symbol &symbol) {
                if (!module_) {
                    module_ = module;
                } else if (std::strcmp(module_, module) != 0) {
                    ::Mln::Sdk::Native::Export::invalid_native_call(
                        "os PUBLISH de um mesmo provider declaram modulos "
                        "diferentes");
                }

                for (const Symbol &registered : symbols_)
                    if (std::strcmp(registered.name, symbol.name) == 0)
                        ::Mln::Sdk::Native::Export::invalid_native_call(
                            "the provider declares the same symbol twice");

                symbols_.push_back(symbol);
            }

            const Module *publish() {
                if (!module_)
                    ::Mln::Sdk::Native::Export::invalid_native_call(
                        "the provider does not declare any PUBLISH symbol");

                module_value_ = Module{ABI_VERSION, sizeof(Module), module_,
                                       static_cast<uint32_t>(symbols_.size()), symbols_.data()};
                return &module_value_;
            }

        private:
            const char *module_ = nullptr;
            std::vector<Symbol> symbols_;
            Module module_value_{};
    };

    struct Registration {
            Registration(const char *module, const Symbol &symbol) {
                Provider::instance().add(module, symbol);
            }
    };

}

#if defined(_WIN32)
#define MLN_NATIVE_PROVIDER_EXPORT extern "C" __declspec(dllexport)
#else
#define MLN_NATIVE_PROVIDER_EXPORT extern "C" __attribute__((weak, visibility("default")))
#endif

MLN_NATIVE_PROVIDER_EXPORT const ::Mln::Sdk::Native::Module *mln_native_module() {
    return ::Mln::Sdk::Native::Export::Provider::instance().publish();
}

#define PUBLISH_RAW(module, name)                                                                  \
    static void name(::Mln::Sdk::Native::Context *context, ::Mln::Sdk::Native::Call *call);        \
    namespace {                                                                                    \
        const ::Mln::Sdk::Native::Export::Registration name##_mln_registration{                    \
            #module, ::Mln::Sdk::Native::Symbol{#name, 1, &name, nullptr}};                        \
    }                                                                                              \
    static void name(::Mln::Sdk::Native::Context *context, ::Mln::Sdk::Native::Call *call)

namespace Mln::Sdk::Native::Export {

    template <typename T> struct arg_storage {
            using type = clean_t<T>;
    };

    template <typename T, std::size_t Extent> struct arg_storage<std::span<const T, Extent>> {
            struct type {
                    std::vector<T> owned;
                    const T *data = nullptr;
                    std::size_t size = 0;
            };
    };

    template <typename T>
    typename arg_storage<T>::type read_argument(::Mln::Sdk::Native::Context *context,
                                                uint64_t raw) {
        using A = clean_t<T>;
        if constexpr (std::is_same_v<A, int32_t>) {
            return std::bit_cast<int32_t>(static_cast<uint32_t>(raw));
        } else if constexpr (std::is_same_v<A, int64_t>) {
            return std::bit_cast<int64_t>(raw);
        } else if constexpr (std::is_same_v<A, uint32_t>) {
            return static_cast<uint32_t>(raw);
        } else if constexpr (std::is_same_v<A, uint64_t>) {
            return raw;
        } else if constexpr (std::is_same_v<A, float>) {
            return std::bit_cast<float>(static_cast<uint32_t>(raw));
        } else if constexpr (std::is_same_v<A, double>) {
            return std::bit_cast<double>(raw);
        } else if constexpr (is_buffer_v<A>) {
            return Argument<A>::read(context, static_cast<uint32_t>(raw), 0);
        } else if constexpr (is_span_const_v<A>) {
            using U = typename A::value_type;
            static_assert(std::is_trivially_copyable_v<U> && !std::is_same_v<U, bool>,
                          "PUBLISH: std::span<const T> requires a trivially copyable, non-bool T "
                          "because it views or copies raw Buffer bytes");
            uint32_t id = static_cast<uint32_t>(raw);
            uint32_t bytes = context->buffer.length(context->user_data, id);
            if (bytes % sizeof(U) != 0)
                ::Mln::Sdk::Native::Export::invalid_native_call(
                    "input Buffer size is not a multiple of span element size");
            typename arg_storage<A>::type storage;
            storage.size = bytes / sizeof(U);
            const void *direct =
                context->buffer.data ? context->buffer.data(context->user_data, id) : nullptr;
            if (direct && reinterpret_cast<uintptr_t>(direct) % alignof(U) == 0) {
                storage.data = static_cast<const U *>(direct);
            } else {
                storage.owned.resize(storage.size);
                if (bytes != 0)
                    context->buffer.read(context->user_data, id, 0, storage.owned.data(), bytes);
                storage.data = storage.owned.data();
            }
            return storage;
        } else if constexpr (is_structured_value_v<A>) {
            uint32_t id = static_cast<uint32_t>(raw);
            uint32_t bytes = context->buffer.length(context->user_data, id);
            std::vector<uint8_t> encoded(bytes);
            if (bytes != 0)
                context->buffer.read(context->user_data, id, 0, encoded.data(), bytes);
            ::Mln::Sdk::Native::Argument<void>::Reader reader(std::as_bytes(std::span(encoded)));
            A value = reader.template read<A>();
            if (!reader.empty())
                ::Mln::Sdk::Native::Export::invalid_native_call(
                    "Return<T> did not consume the entire argument Buffer");
            return value;
        } else {
            static_assert(is_valid_arg_v<A>,
                          "PUBLISH: unsupported argument; use a Wasm scalar, std::span<const T>, "
                          "Mln::Buffer, or a type with Native::Argument<T> and "
                          "Native::Return<T>");
        }
    }

    template <typename T> auto make_call_arg(typename arg_storage<T>::type &storage) {
        using A = clean_t<T>;
        if constexpr (is_span_const_v<A>) {
            return A(storage.data, storage.size);
        } else {
            return storage;
        }
    }

    template <typename T>
    void write_return(::Mln::Sdk::Native::Context *context, ::Mln::Sdk::Native::Call *call,
                      T &&result) {
        using R = clean_t<T>;
        if constexpr (std::is_same_v<R, int32_t>) {
            call->results[0] = std::bit_cast<uint32_t>(result);
        } else if constexpr (std::is_same_v<R, int64_t>) {
            call->results[0] = std::bit_cast<uint64_t>(result);
        } else if constexpr (std::is_same_v<R, uint32_t> || std::is_same_v<R, uint64_t>) {
            call->results[0] = static_cast<uint64_t>(result);
        } else if constexpr (std::is_same_v<R, float>) {
            call->results[0] = std::bit_cast<uint32_t>(result);
        } else if constexpr (std::is_same_v<R, double>) {
            call->results[0] = std::bit_cast<uint64_t>(result);
        } else if constexpr (is_buffer_v<R>) {
            Return<R>::write(context, call, std::forward<T>(result), 0);
        } else if constexpr (is_structured_value_v<R>) {
            ::Mln::Sdk::Native::Return<void>::Writer writer;
            writer.write(result);
            if (writer.size() > std::numeric_limits<uint32_t>::max())
                ::Mln::Sdk::Native::Export::invalid_native_call(
                    "encoded result is too large for a 32-bit Buffer");
            uint32_t bytes = static_cast<uint32_t>(writer.size());
            uint32_t id = context->buffer.allocate(context->user_data, bytes);
            if (bytes != 0)
                context->buffer.write(context->user_data, id, 0, writer.bytes().data(), bytes);
            call->results[0] = id;
        } else {
            static_assert(is_valid_result_v<R>,
                          "PUBLISH: unsupported return; use void, a Wasm scalar, Mln::Buffer, "
                          "or a type with Native::Argument<T> and Native::Return<T>");
        }
    }

    template <typename T>
    inline constexpr ::Mln::Sdk::Native::Value::Kind native_value_kind_v = [] {
        using P = physical_type_t<T>;
        if constexpr (std::is_same_v<P, uint32_t>)
            return ::Mln::Sdk::Native::Value::Kind::i32;
        if constexpr (std::is_same_v<P, uint64_t>)
            return ::Mln::Sdk::Native::Value::Kind::i64;
        if constexpr (std::is_same_v<P, float>)
            return ::Mln::Sdk::Native::Value::Kind::f32;
        if constexpr (std::is_same_v<P, double>)
            return ::Mln::Sdk::Native::Value::Kind::f64;
    }();

    template <auto Fn> struct Trampoline;

    template <typename Ret, typename... Args, Ret (*Fn)(Args...)> struct Trampoline<Fn> {
            static_assert((is_valid_arg_v<Args> && ...),
                          "PUBLISH: unsupported argument; use a Wasm scalar, std::span<const T>, "
                          "Mln::Buffer, or a type with Native::Argument<T> and "
                          "Native::Return<T>");
            static_assert(is_valid_result_v<Ret>,
                          "PUBLISH: unsupported return; use void, a Wasm scalar, Mln::Buffer, "
                          "or a type with Native::Argument<T> and Native::Return<T>");

            static inline constexpr std::array<::Mln::Sdk::Native::Value::Kind, sizeof...(Args)>
                argument_kinds{native_value_kind_v<Args>...};
            static inline constexpr std::array<::Mln::Sdk::Native::Value::Kind,
                                               std::is_void_v<Ret> ? 0 : 1>
                result_kinds = [] {
                    if constexpr (std::is_void_v<Ret>)
                        return std::array<::Mln::Sdk::Native::Value::Kind, 0>{};
                    else
                        return std::array<::Mln::Sdk::Native::Value::Kind, 1>{
                            native_value_kind_v<Ret>};
                }();
            static inline constexpr ::Mln::Sdk::Native::Signature signature{
                static_cast<uint32_t>(argument_kinds.size()), argument_kinds.data(),
                static_cast<uint32_t>(result_kinds.size()), result_kinds.data()};

            static void invoke(::Mln::Sdk::Native::Context *context,
                               ::Mln::Sdk::Native::Call *call) {
                constexpr uint32_t expected_results = std::is_void_v<Ret> ? 0u : 1u;
                if (call->argc != sizeof...(Args) || call->resultc != expected_results)
                    ::Mln::Sdk::Native::Export::invalid_native_call(
                        "argument or result count does not match the native contract");
                invoke_impl(context, call, std::index_sequence_for<Args...>{});
            }

        private:
            template <std::size_t... I>
            static void invoke_impl(::Mln::Sdk::Native::Context *context,
                                    ::Mln::Sdk::Native::Call *call, std::index_sequence<I...>) {
                std::tuple<typename arg_storage<Args>::type...> storage{
                    read_argument<Args>(context, call->args[I])...};
                if constexpr (std::is_void_v<Ret>) {
                    Fn(make_call_arg<Args>(std::get<I>(storage))...);
                } else {
                    Ret result = Fn(make_call_arg<Args>(std::get<I>(storage))...);
                    write_return(context, call, std::move(result));
                }
            }
    };

}

#ifdef PUBLISH
#undef PUBLISH
#endif
#define PUBLISH(module, name, ReturnType, ...)                                                     \
    static ReturnType name(__VA_ARGS__);                                                           \
    namespace {                                                                                    \
        using name##_mln_trampoline = ::Mln::Sdk::Native::Export::Trampoline<&name>;               \
        const ::Mln::Sdk::Native::Export::Registration name##_mln_registration{                    \
            #module, ::Mln::Sdk::Native::Symbol{#name, 1, &name##_mln_trampoline::invoke,          \
                                                &name##_mln_trampoline::signature}};               \
    }                                                                                              \
    static ReturnType name(__VA_ARGS__)
