#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Power : public Binary<Kind::Power> {
        public:
            Power(Tensor left, Tensor right)
                : Binary<Kind::Power>(std::move(left), std::move(right)) {}
    };
}
