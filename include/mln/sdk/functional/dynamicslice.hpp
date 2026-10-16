#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DynamicSlice : public Operation::Of<Kind::DynamicSlice> {
        public:
            Tensor x;
            std::vector<Tensor> start_indices;
            std::vector<int64_t> slice_sizes;

            DynamicSlice(Tensor input, std::vector<Tensor> indices, std::vector<int64_t> sizes)
                : x(std::move(input)), start_indices(std::move(indices)),
                  slice_sizes(std::move(sizes)) {}

            std::vector<int64_t> signature() const override { return slice_sizes; }
    };
}
