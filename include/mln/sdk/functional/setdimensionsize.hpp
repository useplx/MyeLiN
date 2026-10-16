#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class SetDimensionSize : public Operation::Of<Kind::SetDimensionSize> {
        public:
            Tensor x;
            Tensor size;
            int64_t dimension;

            SetDimensionSize(Tensor input, Tensor extent, int64_t axis)
                : x(std::move(input)), size(std::move(extent)), dimension(axis) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> result = x.shape;
                if (dimension >= 0 && static_cast<std::size_t>(dimension) < result.size())
                    result[static_cast<std::size_t>(dimension)] = Tensor::Dynamic;
                return result;
            }
    };
}
