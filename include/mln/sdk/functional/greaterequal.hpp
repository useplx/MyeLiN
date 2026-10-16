#pragma once

#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class GreaterEqual : public Binary<Kind::GreaterEqual> {
        public:
            GreaterEqual(Tensor left, Tensor right)
                : Binary<Kind::GreaterEqual>(std::move(left), std::move(right)) {}
    };
}
