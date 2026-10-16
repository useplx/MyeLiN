#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Remainder : public Binary<Kind::Remainder> {
        public:
            Remainder(Tensor left, Tensor right)
                : Binary<Kind::Remainder>(std::move(left), std::move(right)) {}
    };
}
