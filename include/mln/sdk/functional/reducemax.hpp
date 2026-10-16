#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class ReduceMax : public Operation::Of<Kind::ReduceMax> {
        public:
            Tensor x;
            std::vector<int64_t> axes;

            ReduceMax(Tensor input, std::vector<int64_t> dimensions)
                : x(std::move(input)), axes(std::move(dimensions)) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> shape;
                for (std::size_t i = 0; i < x.shape.size(); i++) {
                    bool reduced = false;
                    for (int64_t axis : axes)
                        reduced = reduced || axis == static_cast<int64_t>(i);
                    if (!reduced)
                        shape.push_back(x.shape[i]);
                }
                return shape;
            }
    };
}
