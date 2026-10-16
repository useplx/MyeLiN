#pragma once

#include "mln/sdk/operation.hpp"

#include <algorithm>

namespace Mln::Sdk::Functional {
    class Pad : public Operation::Of<Kind::Pad> {
        public:
            Tensor x;
            Tensor value;
            std::vector<int64_t> low;
            std::vector<int64_t> high;
            std::vector<int64_t> interior;

            Pad(Tensor input, Tensor padding_value, std::vector<int64_t> edge_low,
                std::vector<int64_t> edge_high, std::vector<int64_t> inside)
                : x(std::move(input)), value(std::move(padding_value)), low(std::move(edge_low)),
                  high(std::move(edge_high)), interior(std::move(inside)) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> result = x.shape;
                for (std::size_t i = 0; i < result.size(); ++i) {
                    if (result[i] == Tensor::Dynamic)
                        continue;
                    result[i] = low[i] + high[i] + result[i] +
                                interior[i] * std::max<int64_t>(result[i] - 1, 0);
                }
                return result;
            }
    };
}
