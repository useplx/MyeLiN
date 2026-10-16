#pragma once

#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Equal : public Binary<Kind::Equal> {
        public:
            Equal(Tensor left, Tensor right)
                : Binary<Kind::Equal>(std::move(left), std::move(right)) {}
    };
}
