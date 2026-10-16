#pragma once
#include "mln/sdk/functional/binary.hpp"

namespace Mln::Sdk::Functional {
    class BitwiseXor : public Binary<Kind::BitwiseXor> {
        public:
            BitwiseXor(Tensor left, Tensor right)
                : Binary<Kind::BitwiseXor>(std::move(left), std::move(right)) {}
    };
}
