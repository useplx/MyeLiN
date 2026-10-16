#pragma once

#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Greater : public Binary<Kind::Greater> {
        public:
            Greater(Tensor left, Tensor right)
                : Binary<Kind::Greater>(std::move(left), std::move(right)) {}
    };
}
