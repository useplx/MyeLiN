#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Einsum : public Operation::Of<Kind::Einsum> {
        public:
            Tensor lhs;
            Tensor rhs;
            std::string equation;
            std::vector<int64_t> shape;

            Einsum(Tensor left, Tensor right, std::string config, std::vector<int64_t> result_shape)
                : lhs(std::move(left)), rhs(std::move(right)), equation(std::move(config)),
                  shape(std::move(result_shape)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
