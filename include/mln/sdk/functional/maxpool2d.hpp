#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class MaxPool2D : public Operation::Of<Kind::MaxPool2D> {
        public:
            Tensor x;
            std::vector<int64_t> kernel, stride, padding;

            MaxPool2D(Tensor input, std::vector<int64_t> window, std::vector<int64_t> strides,
                      std::vector<int64_t> pads)
                : x(std::move(input)), kernel(std::move(window)), stride(std::move(strides)),
                  padding(std::move(pads)) {}

            std::vector<int64_t> signature() const override {
                int64_t H_out = (x.shape[2] + padding[0] + padding[1] - kernel[0]) / stride[0] + 1;
                int64_t W_out = (x.shape[3] + padding[2] + padding[3] - kernel[1]) / stride[1] + 1;
                return {x.shape[0], x.shape[1], H_out, W_out};
            }
    };
}
