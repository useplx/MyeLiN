#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    template <Kind K> class Unary : public Operation::Of<K> {
        public:
            Tensor x;

            explicit Unary(Tensor input) : x(std::move(input)) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
