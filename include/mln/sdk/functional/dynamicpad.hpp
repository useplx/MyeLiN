#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DynamicPad : public Operation::Of<Kind::DynamicPad> {
        public:
            Tensor x;
            Tensor value;
            Tensor low;
            Tensor high;
            Tensor interior;
            std::vector<int64_t> shape;

            DynamicPad(Tensor input, Tensor padding_value, Tensor edge_low, Tensor edge_high,
                       Tensor inside, std::vector<int64_t> result_shape)
                : x(std::move(input)), value(std::move(padding_value)), low(std::move(edge_low)),
                  high(std::move(edge_high)), interior(std::move(inside)),
                  shape(std::move(result_shape)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
