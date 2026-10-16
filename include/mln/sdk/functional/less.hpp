#pragma once

#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Less : public Binary<Kind::Less> {
        public:
            Less(Tensor left, Tensor right)
                : Binary<Kind::Less>(std::move(left), std::move(right)) {}
    };
}
