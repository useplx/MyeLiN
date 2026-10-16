#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class UnaryEinsum : public Operation::Of<Kind::UnaryEinsum> {
        public:
            Tensor x;
            std::string equation;
            std::vector<int64_t> shape;

            UnaryEinsum(Tensor input, std::string config, std::vector<int64_t> result_shape)
                : x(std::move(input)), equation(std::move(config)), shape(std::move(result_shape)) {
            }

            std::vector<int64_t> signature() const override { return shape; }
    };
}
