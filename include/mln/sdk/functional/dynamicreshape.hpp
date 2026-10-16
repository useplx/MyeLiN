#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DynamicReshape : public Operation::Of<Kind::DynamicReshape> {
        public:
            Tensor x;
            Tensor output_shape;
            std::vector<int64_t> shape;

            DynamicReshape(Tensor input, Tensor dimensions, std::vector<int64_t> result_shape)
                : x(std::move(input)), output_shape(std::move(dimensions)),
                  shape(std::move(result_shape)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
