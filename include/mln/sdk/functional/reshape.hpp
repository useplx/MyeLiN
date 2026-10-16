#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Reshape : public Operation::Of<Kind::Reshape> {
        public:
            Tensor x;
            std::vector<int64_t> shape;

            Reshape(Tensor input, std::vector<int64_t> dimensions)
                : x(std::move(input)), shape(std::move(dimensions)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
