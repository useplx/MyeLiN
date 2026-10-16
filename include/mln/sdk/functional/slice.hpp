#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Slice : public Operation::Of<Kind::Slice> {
        public:
            Tensor x;
            Tensor limits_source;
            std::vector<int64_t> limits_source_dimensions;
            std::vector<int64_t> starts, limits, strides;

            Slice(Tensor input, std::vector<int64_t> start_values,
                  std::vector<int64_t> limit_values, std::vector<int64_t> stride_values)
                : x(std::move(input)), starts(std::move(start_values)),
                  limits(std::move(limit_values)), strides(std::move(stride_values)) {}

            Slice(Tensor input, std::vector<int64_t> start_values,
                  std::vector<int64_t> limit_values, std::vector<int64_t> stride_values,
                  Tensor limit_source, std::vector<int64_t> source_dimensions)
                : x(std::move(input)), limits_source(std::move(limit_source)),
                  limits_source_dimensions(std::move(source_dimensions)),
                  starts(std::move(start_values)), limits(std::move(limit_values)),
                  strides(std::move(stride_values)) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> shape(starts.size());
                for (std::size_t i = 0; i < starts.size(); i++)
                    shape[i] = limits[i] == Tensor::Dynamic
                                   ? Tensor::Dynamic
                                   : (limits[i] - starts[i] + strides[i] - 1) / strides[i];
                return shape;
            }
    };
}
