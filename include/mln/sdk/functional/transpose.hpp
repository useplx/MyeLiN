#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Transpose : public Operation::Of<Kind::Transpose> {
        public:
            Tensor x;
            std::vector<int64_t> permutation;

            Transpose(Tensor input, std::vector<int64_t> order)
                : x(std::move(input)), permutation(std::move(order)) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> shape(permutation.size());
                for (std::size_t i = 0; i < permutation.size(); i++)
                    shape[i] = x.shape[permutation[i]];
                return shape;
            }
    };
}
