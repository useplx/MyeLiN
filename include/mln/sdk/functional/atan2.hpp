#pragma once
#include "mln/sdk/functional/binary.hpp"
namespace Mln::Sdk::Functional {
    class Atan2 : public Binary<Kind::Atan2> {
        public:
            Atan2(Tensor left, Tensor right)
                : Binary<Kind::Atan2>(std::move(left), std::move(right)) {}
    };
}
