#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Dot : public Operation::Of<Kind::Dot> {
        public:
            Tensor lhs;
            Tensor rhs;
            std::vector<int64_t> shape;

            Dot(Tensor left, Tensor right, std::vector<int64_t> output_shape)
                : lhs(std::move(left)), rhs(std::move(right)), shape(std::move(output_shape)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
