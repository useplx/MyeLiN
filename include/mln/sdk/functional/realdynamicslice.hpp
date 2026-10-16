#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class RealDynamicSlice : public Operation::Of<Kind::RealDynamicSlice> {
        public:
            Tensor x;
            Tensor starts;
            Tensor limits;
            Tensor strides;
            std::vector<int64_t> shape;

            RealDynamicSlice(Tensor input, Tensor start_indices, Tensor limit_indices,
                             Tensor step_sizes, std::vector<int64_t> result_shape)
                : x(std::move(input)), starts(std::move(start_indices)),
                  limits(std::move(limit_indices)), strides(std::move(step_sizes)),
                  shape(std::move(result_shape)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
