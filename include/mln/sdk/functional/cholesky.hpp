#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Cholesky : public Operation::Of<Kind::Cholesky> {
        public:
            Tensor x;
            bool lower;

            Cholesky(Tensor input, bool use_lower = false)
                : x(std::move(input)), lower(use_lower) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
