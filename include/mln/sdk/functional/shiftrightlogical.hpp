#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class ShiftRightLogical : public Binary<Kind::ShiftRightLogical> {
        public:
            ShiftRightLogical(Tensor left, Tensor right)
                : Binary<Kind::ShiftRightLogical>(std::move(left), std::move(right)) {}
    };
}
