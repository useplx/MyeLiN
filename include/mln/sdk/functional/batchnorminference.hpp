#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class BatchNormInference : public Operation::Of<Kind::BatchNormInference> {
        public:
            Tensor x;
            Tensor scale;
            Tensor offset;
            Tensor mean;
            Tensor variance;
            float epsilon;
            int64_t feature_index;

            BatchNormInference(Tensor input, Tensor weight, Tensor bias, Tensor running_mean,
                               Tensor running_variance, float stability, int64_t feature_axis)
                : x(std::move(input)), scale(std::move(weight)), offset(std::move(bias)),
                  mean(std::move(running_mean)), variance(std::move(running_variance)),
                  epsilon(stability), feature_index(feature_axis) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
