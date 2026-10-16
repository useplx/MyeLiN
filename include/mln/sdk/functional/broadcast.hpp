#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Broadcast : public Operation::Of<Kind::Broadcast> {
        public:
            Tensor x;
            Tensor shape_source;
            std::vector<int64_t> shape, dims;

            Broadcast(Tensor input, std::vector<int64_t> target_shape,
                      std::vector<int64_t> dimensions)
                : x(std::move(input)), shape(std::move(target_shape)), dims(std::move(dimensions)) {
            }

            Broadcast(Tensor input, std::vector<int64_t> target_shape,
                      std::vector<int64_t> dimensions, Tensor source)
                : x(std::move(input)), shape_source(std::move(source)),
                  shape(std::move(target_shape)), dims(std::move(dimensions)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
