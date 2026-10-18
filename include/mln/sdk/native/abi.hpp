#pragma once

#include <cstdint>

namespace Mln::Sdk::Native {
    namespace Value {
        enum class Kind : uint8_t {
            i32,
            i64,
            f32,
            f64,
        };
    }

    struct Signature {
            uint32_t argument_count;
            const Value::Kind *arguments;
            uint32_t result_count;
            const Value::Kind *results;
    };

    namespace Buffer {
        struct Context {
                uint32_t (*allocate)(void *user_data, uint32_t bytes);
                void (*release)(void *user_data, uint32_t buffer);
                uint32_t (*length)(void *user_data, uint32_t buffer);
                void (*read)(void *user_data, uint32_t buffer, uint32_t offset, void *dst,
                             uint32_t len);
                void (*write)(void *user_data, uint32_t buffer, uint32_t offset, const void *src,
                              uint32_t len);
                const void *(*data)(void *user_data, uint32_t buffer);
        };
    }

    struct Context {
            void *user_data;
            Buffer::Context buffer;
    };

    struct Call {
            const uint64_t *args;
            uint32_t argc;

            uint64_t *results;
            uint32_t resultc;
    };

    using Function = void (*)(Context *context, Call *call);

    struct Symbol {
            const char *name;
            uint32_t version;
            Function function;
            const Signature *signature;
    };

    struct Module {
            uint32_t abi_version;
            uint32_t struct_size;
            const char *name;

            uint32_t symbol_count;
            const Symbol *symbols;
    };

    inline constexpr uint32_t ABI_VERSION = 6;
    inline constexpr uint32_t MIN_COMPATIBLE_ABI_VERSION = 6;
}

#if defined(_WIN32) && defined(MLN_NATIVE)
extern "C" __declspec(dllexport) const ::Mln::Sdk::Native::Module *mln_native_module();
#else
extern "C" const ::Mln::Sdk::Native::Module *mln_native_module();
#endif
