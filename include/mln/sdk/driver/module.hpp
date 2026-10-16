#pragma once

#include <cstddef>
#include <cstdint>

namespace Mln::Sdk::Driver {
    inline constexpr std::uint32_t AbiVersion = 3;

    enum class Value : std::uint8_t { Boolean, Integer, Number, String };

    struct Option {
            const char *name;
            Value type;
            bool required;
    };

    struct Setting {
            const char *name;
            Value type;
            bool boolean;
            std::int64_t integer;
            double number;
            const char *string;
    };

    struct Function {
            const char *name;
            std::uint16_t constant_count;
            std::uint16_t binding_count;
    };

    struct Binding {
            void *data;
            std::size_t size;
    };

    struct Module {
            std::uint32_t abi_version;
            std::uint32_t struct_size;
            const char *name;
            const char *format;
            void *(*load)(const std::uint8_t *data, std::size_t size, char *error,
                          std::size_t error_capacity);
            void (*destroy)(void *executable);
            std::size_t (*function_count)(void *executable);
            std::int32_t (*function)(void *executable, std::size_t ordinal, Function *result,
                                     char *error, std::size_t error_capacity);
            std::int32_t (*invoke)(void *executable, std::size_t ordinal,
                                   const std::uint32_t *constants, std::size_t constant_count,
                                   const Binding *bindings, std::size_t binding_count, char *error,
                                   std::size_t error_capacity);
            std::size_t option_count;
            const Option *options;
            std::int32_t (*configure)(const Setting *settings, std::size_t setting_count,
                                      char *error, std::size_t error_capacity);
    };
}
