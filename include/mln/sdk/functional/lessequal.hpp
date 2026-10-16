#pragma once

#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class LessEqual : public Binary<Kind::LessEqual> {
        public:
            LessEqual(Tensor left, Tensor right)
                : Binary<Kind::LessEqual>(std::move(left), std::move(right)) {}
    };
}
