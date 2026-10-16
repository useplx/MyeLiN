#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DynamicBroadcast : public Operation::Of<Kind::DynamicBroadcast> {
        public:
            Tensor x;
            Tensor output_dimensions;
            std::vector<int64_t> shape;
            std::vector<int64_t> dimensions;

            DynamicBroadcast(Tensor input, Tensor output_shape, std::vector<int64_t> result_shape,
                             std::vector<int64_t> broadcast_dimensions)
                : x(std::move(input)), output_dimensions(std::move(output_shape)),
                  shape(std::move(result_shape)), dimensions(std::move(broadcast_dimensions)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
