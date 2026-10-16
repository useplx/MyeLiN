#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class BatchNormGrad : public Operation::Of<Kind::BatchNormGrad> {
        public:
            Tensor x;
            Tensor scale;
            Tensor mean;
            Tensor variance;
            Tensor grad_output;
            float epsilon;
            int64_t feature_index;

            BatchNormGrad(Tensor input, Tensor weight, Tensor batch_mean, Tensor batch_variance,
                          Tensor output_gradient, float stability, int64_t feature_axis)
                : x(std::move(input)), scale(std::move(weight)), mean(std::move(batch_mean)),
                  variance(std::move(batch_variance)), grad_output(std::move(output_gradient)),
                  epsilon(stability), feature_index(feature_axis) {}

            std::vector<int64_t> signature() const override { return x.shape; }

            std::vector<std::vector<int64_t>> signatures() const override {
                return {x.shape, scale.shape, scale.shape};
            }
    };
}
