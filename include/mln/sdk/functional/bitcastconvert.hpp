#pragma once

#include "mln/dtype.hpp"
#include "mln/sdk/operation.hpp"

#include <utility>
#include <vector>

namespace Mln::Sdk::Functional {
    class BitcastConvert : public Operation::Of<Kind::BitcastConvert> {
        public:
            Tensor x;
            DType target;
            std::vector<int64_t> shape;

            BitcastConvert(Tensor input, DType element_type, std::vector<int64_t> dimensions)
                : x(std::move(input)), target(element_type), shape(std::move(dimensions)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
