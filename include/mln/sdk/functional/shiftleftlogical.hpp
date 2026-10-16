#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class ShiftLeftLogical : public Binary<Kind::ShiftLeftLogical> {
        public:
            ShiftLeftLogical(Tensor left, Tensor right)
                : Binary<Kind::ShiftLeftLogical>(std::move(left), std::move(right)) {}
    };
}
