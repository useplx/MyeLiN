#pragma once

#include "mln/sdk/native/buffer.hpp"
#include "mln/sdk/native/export.hpp"
#include "mln/sdk/native/session.hpp"
#include "mln/sdk/orchestration.hpp"

#include <cstdint>
#include <type_traits>

#if defined(__wasm__)
#define ENTRYPOINT(Type)                                                                           \
    static_assert(std::is_base_of_v<::Mln::Sdk::Orchestration, Type>,                              \
                  "ENTRYPOINT requires a Mln::Sdk::Orchestration type");                           \
    extern "C" __attribute__((export_name("forward"))) uint32_t mln_wasm_forward(uint32_t session, \
                                                                                 uint32_t input) { \
        auto mln_session = ::Mln::Sdk::Native::Argument<::Mln::Runtime::Session>::read(session);   \
        return ::Mln::Sdk::Native::Return<::Mln::Buffer>::write(Type{}.forward(                    \
            mln_session, ::Mln::Sdk::Native::Argument<::Mln::Buffer>::read(input)));               \
    }
#else
#define ENTRYPOINT(Type)                                                                           \
    static_assert(std::is_base_of_v<::Mln::Sdk::Orchestration, Type>,                              \
                  "ENTRYPOINT requires a Mln::Sdk::Orchestration type");                           \
    namespace {                                                                                    \
        void mln_native_forward(::Mln::Sdk::Native::Context *context,                              \
                                ::Mln::Sdk::Native::Call *call) {                                  \
            if (call->argc != 2 || call->resultc != 1)                                             \
                ::Mln::Sdk::Native::Export::invalid_native_call("invalid entrypoint arity");       \
            auto session =                                                                         \
                ::Mln::Runtime::Session::borrow(*context, static_cast<uint32_t>(call->args[0]));   \
            auto input = ::Mln::Sdk::Native::Argument<::Mln::Buffer>::read(                        \
                context, static_cast<uint32_t>(call->args[1]), 0);                                 \
            ::Mln::Sdk::Native::Return<::Mln::Buffer>::write(                                      \
                context, call, Type{}.forward(session, std::move(input)), 0);                      \
        }                                                                                          \
        const ::Mln::Sdk::Native::Export::Registration mln_entrypoint_registration{                \
            "entrypoint", ::Mln::Sdk::Native::Symbol{"forward", 1, &mln_native_forward, nullptr}}; \
    }
#endif
