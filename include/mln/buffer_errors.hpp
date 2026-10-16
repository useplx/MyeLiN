#pragma once

#include "mln/buffer.hpp"
#include "mln/error.hpp"

namespace Mln {
    struct Buffer::Errors {
            struct Abi {
                    struct Size {
                            inline static constexpr Error Overflow{
                                Range::Buffer + 0, "Buffer size {} exceeds the 32-bit ABI limit"};
                    };
                    struct Client {
                            inline static constexpr Error Required{
                                Range::Buffer + 1,
                                "ABI Buffer access is only available inside an ABI client"};
                    };
                    struct Identifier {
                            inline static constexpr Error Unavailable{
                                Range::Buffer + 2, "a local Buffer has no ABI identifier"};
                    };
                    struct Ownership {
                            struct Transfer {
                                    inline static constexpr Error Unavailable{
                                        Range::Buffer + 3,
                                        "a local Buffer cannot transfer ownership to the ABI"};
                            };
                    };
            };
            struct Native {
                    struct Inside {
                            struct Wasm {
                                    inline static constexpr Error Unavailable{
                                        Range::Buffer + 4,
                                        "Native Buffer access is unavailable inside Wasm"};
                            };
                    };
            };
            struct Runtime {
                    struct Lifetime {
                            inline static constexpr Error Ended{Range::Buffer + 5,
                                                                "the Runtime that owns this Buffer "
                                                                "representation no longer exists"};
                    };
            };
            struct Read {
                    struct Bounds {
                            inline static constexpr Error Exceeded{
                                Range::Buffer + 6,
                                "Buffer read at offset {} with size {} exceeds storage size {}"};
                    };
                    struct Storage {
                            inline static constexpr Error Missing{
                                Range::Buffer + 7, "cannot read a Buffer without storage"};
                    };
            };
            struct Write {
                    struct Bounds {
                            inline static constexpr Error Exceeded{
                                Range::Buffer + 8,
                                "Buffer write at offset {} with size {} exceeds storage size {}"};
                    };
                    struct Storage {
                            inline static constexpr Error Missing{
                                Range::Buffer + 9, "cannot write a Buffer without storage"};
                    };
            };
    };
}
