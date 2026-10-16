#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Maximum : public Binary<Kind::Maximum> {
        public:
            Maximum(Tensor left, Tensor right)
                : Binary<Kind::Maximum>(std::move(left), std::move(right)) {}
    };
}
