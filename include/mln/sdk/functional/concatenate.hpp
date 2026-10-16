#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Concatenate : public Operation::Of<Kind::Concatenate> {
        public:
            Tensor a, b;
            int64_t dim;

            Concatenate(Tensor left, Tensor right, int64_t dimension)
                : a(std::move(left)), b(std::move(right)), dim(dimension) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> shape = a.shape;
                shape[dim] += b.shape[dim];
                return shape;
            }
    };
}
