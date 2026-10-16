#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Multiply : public Binary<Kind::Multiply> {
        public:
            Multiply(Tensor left, Tensor right)
                : Binary<Kind::Multiply>(std::move(left), std::move(right)) {}
    };
}
