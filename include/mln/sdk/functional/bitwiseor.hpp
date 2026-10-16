#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class BitwiseOr : public Binary<Kind::BitwiseOr> {
        public:
            BitwiseOr(Tensor left, Tensor right)
                : Binary<Kind::BitwiseOr>(std::move(left), std::move(right)) {}
    };
}
