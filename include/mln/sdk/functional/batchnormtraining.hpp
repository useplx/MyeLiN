#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class BatchNormTraining : public Operation::Of<Kind::BatchNormTraining> {
        public:
            Tensor x;
            Tensor scale;
            Tensor offset;
            float epsilon;
            int64_t feature_index;

            BatchNormTraining(Tensor input, Tensor weight, Tensor bias, float stability,
                              int64_t feature_axis)
                : x(std::move(input)), scale(std::move(weight)), offset(std::move(bias)),
                  epsilon(stability), feature_index(feature_axis) {}

            std::vector<int64_t> signature() const override { return x.shape; }

            std::vector<std::vector<int64_t>> signatures() const override {
                return {x.shape, scale.shape, scale.shape};
            }
    };
}
