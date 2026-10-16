#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    template <Kind K> class Binary : public Operation::Of<K> {
        public:
            Tensor lhs, rhs;

            Binary(Tensor left, Tensor right) : lhs(std::move(left)), rhs(std::move(right)) {}

            std::vector<int64_t> signature() const override { return lhs.shape; }
    };
}
