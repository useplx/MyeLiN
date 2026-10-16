#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Minimum : public Binary<Kind::Minimum> {
        public:
            Minimum(Tensor left, Tensor right)
                : Binary<Kind::Minimum>(std::move(left), std::move(right)) {}
    };
}
