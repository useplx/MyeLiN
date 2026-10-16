#pragma once

#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class NotEqual : public Binary<Kind::NotEqual> {
        public:
            NotEqual(Tensor left, Tensor right)
                : Binary<Kind::NotEqual>(std::move(left), std::move(right)) {}
    };
}
