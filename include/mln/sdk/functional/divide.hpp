#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Divide : public Binary<Kind::Divide> {
        public:
            Divide(Tensor left, Tensor right)
                : Binary<Kind::Divide>(std::move(left), std::move(right)) {}
    };
}
