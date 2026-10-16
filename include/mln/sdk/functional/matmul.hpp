#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class MatMul : public Operation::Of<Kind::MatMul> {
        public:
            Tensor lhs, rhs;

            MatMul(Tensor left, Tensor right) : lhs(std::move(left)), rhs(std::move(right)) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> shape(lhs.shape.begin(), lhs.shape.end() - 1);
                if (rhs.shape.size() > 1)
                    shape.push_back(rhs.shape.back());
                return shape;
            }
    };
}
