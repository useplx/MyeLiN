#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    enum class Distribution { Uniform, Normal };

    class Rng : public Operation::Of<Kind::Rng> {
        public:
            Tensor a;
            Tensor b;
            Tensor dimensions;
            std::vector<int64_t> shape;
            Distribution distribution;

            Rng(Tensor first_parameter, Tensor second_parameter, Tensor output_dimensions,
                std::vector<int64_t> output_shape, Distribution distribution_kind)
                : a(std::move(first_parameter)), b(std::move(second_parameter)),
                  dimensions(std::move(output_dimensions)), shape(std::move(output_shape)),
                  distribution(distribution_kind) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
