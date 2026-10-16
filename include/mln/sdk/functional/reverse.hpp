#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Reverse : public Operation::Of<Kind::Reverse> {
        public:
            Tensor x;
            std::vector<int64_t> dimensions;

            Reverse(Tensor input, std::vector<int64_t> axes)
                : x(std::move(input)), dimensions(std::move(axes)) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
