#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {

    class Upsample : public Operation::Of<Kind::Upsample> {
        public:
            Tensor x;
            std::vector<int64_t> scale;

            Upsample(Tensor input, std::vector<int64_t> factors)
                : x(std::move(input)), scale(std::move(factors)) {}

            std::vector<int64_t> signature() const override {
                return {x.shape[0], x.shape[1], x.shape[2] * scale[0], x.shape[3] * scale[1]};
            }
    };
}
