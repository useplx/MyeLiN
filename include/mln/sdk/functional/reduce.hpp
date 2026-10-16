#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    enum class Reduction { Add, Multiply, Maximum, Minimum, And, Or, Xor };

    class Reduce : public Operation::Of<Kind::Reduce> {
        public:
            Tensor x;
            Tensor initial;
            std::vector<int64_t> axes;
            std::vector<int64_t> shape;
            Reduction reduction;

            Reduce(Tensor input, Tensor initial_value, std::vector<int64_t> dimensions,
                   std::vector<int64_t> output_shape, Reduction operation)
                : x(std::move(input)), initial(std::move(initial_value)),
                  axes(std::move(dimensions)), shape(std::move(output_shape)),
                  reduction(operation) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
