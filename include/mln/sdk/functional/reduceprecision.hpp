#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class ReducePrecision : public Operation::Of<Kind::ReducePrecision> {
        public:
            Tensor x;
            int32_t exponent_bits;
            int32_t mantissa_bits;

            ReducePrecision(Tensor input, int32_t exponent, int32_t mantissa)
                : x(std::move(input)), exponent_bits(exponent), mantissa_bits(mantissa) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
