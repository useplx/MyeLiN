#pragma once
#include "mln/sdk/functional/binary.hpp"
namespace Mln::Sdk::Functional {
    class ShiftRightArithmetic : public Binary<Kind::ShiftRightArithmetic> {
        public:
            ShiftRightArithmetic(Tensor left, Tensor right)
                : Binary<Kind::ShiftRightArithmetic>(std::move(left), std::move(right)) {}
    };
}
