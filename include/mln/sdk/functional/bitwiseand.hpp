#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class BitwiseAnd : public Binary<Kind::BitwiseAnd> {
        public:
            BitwiseAnd(Tensor left, Tensor right)
                : Binary<Kind::BitwiseAnd>(std::move(left), std::move(right)) {}
    };
}
