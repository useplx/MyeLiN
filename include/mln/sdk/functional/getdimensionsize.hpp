#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class GetDimensionSize : public Operation::Of<Kind::GetDimensionSize> {
        public:
            Tensor x;
            int64_t dimension;

            GetDimensionSize(Tensor input, int64_t axis) : x(std::move(input)), dimension(axis) {}

            std::vector<int64_t> signature() const override { return {}; }
    };
}
