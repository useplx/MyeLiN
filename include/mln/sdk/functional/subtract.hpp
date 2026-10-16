#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Subtract : public Binary<Kind::Subtract> {
        public:
            Subtract(Tensor left, Tensor right)
                : Binary<Kind::Subtract>(std::move(left), std::move(right)) {}
    };
}
