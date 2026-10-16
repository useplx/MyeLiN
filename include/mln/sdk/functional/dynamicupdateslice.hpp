#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DynamicUpdateSlice : public Operation::Of<Kind::DynamicUpdateSlice> {
        public:
            Tensor x;
            Tensor update;
            std::vector<Tensor> start_indices;

            DynamicUpdateSlice(Tensor input, Tensor replacement, std::vector<Tensor> indices)
                : x(std::move(input)), update(std::move(replacement)),
                  start_indices(std::move(indices)) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
