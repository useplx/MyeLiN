#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class Add : public Binary<Kind::Add> {
        public:
            Add(Tensor left, Tensor right) : Binary<Kind::Add>(std::move(left), std::move(right)) {}
    };
}
