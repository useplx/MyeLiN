#pragma once

#include "mln/dtype.hpp"
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DynamicIota : public Operation::Of<Kind::DynamicIota> {
        public:
            Tensor output_shape;
            std::vector<int64_t> shape;
            int64_t dimension;
            DType dtype;

            DynamicIota(Tensor dimensions, std::vector<int64_t> result_shape,
                        int64_t varying_dimension, DType element_type)
                : output_shape(std::move(dimensions)), shape(std::move(result_shape)),
                  dimension(varying_dimension), dtype(element_type) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
