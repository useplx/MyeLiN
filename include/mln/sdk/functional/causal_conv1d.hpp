#pragma once

#include "mln/sdk/operation.hpp"

#include <utility>

namespace Mln::Sdk::Functional {

    class CausalConv1D : public Operation::Of<Kind::CausalConv1D> {
        public:
            Tensor input;
            Tensor kernel;
            Tensor state;

            CausalConv1D(Tensor input_value, Tensor kernel_value, Tensor state_value)
                : input(std::move(input_value)), kernel(std::move(kernel_value)),
                  state(std::move(state_value)) {}

            std::vector<int64_t> signature() const override {
                return {input.shape[0] * input.shape[1] + state.shape[0] * state.shape[1]};
            }
    };
}
