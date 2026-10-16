#pragma once
#include "mln/dtype.hpp"
#include "mln/sdk/operation.hpp"

#include <utility>
#include <vector>

namespace Mln::Sdk::Functional {
    class Iota : public Operation::Of<Kind::Iota> {
        public:
            std::vector<int64_t> shape;
            int64_t dimension;
            DType dtype;

            Iota(std::vector<int64_t> dimensions, int64_t varying_dimension, DType element_type)
                : shape(std::move(dimensions)), dimension(varying_dimension), dtype(element_type) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
