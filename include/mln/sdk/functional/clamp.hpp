#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Clamp : public Operation::Of<Kind::Clamp> {
        public:
            Tensor minimum;
            Tensor x;
            Tensor maximum;

            Clamp(Tensor lower, Tensor input, Tensor upper)
                : minimum(std::move(lower)), x(std::move(input)), maximum(std::move(upper)) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
