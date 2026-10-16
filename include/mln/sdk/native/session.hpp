#pragma once

#include "mln/runtime/runtime.hpp"
#include "mln/sdk/native/serialization.hpp"

#include <cstdint>

namespace Mln::Sdk::Native {
    template <> struct Argument<Mln::Runtime::Session> {
            [[nodiscard]] static Mln::Runtime::Session read(std::uint32_t identifier) {
                return Mln::Runtime::Session(identifier);
            }
    };
}
