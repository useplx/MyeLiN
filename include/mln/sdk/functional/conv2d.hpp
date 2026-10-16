#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Conv2D : public Operation::Of<Kind::Conv2D> {
        public:
            Tensor x, weight, bias;
            std::vector<int64_t> stride, padding, dilation;
            int64_t groups;

            Conv2D(Tensor input, Tensor kernel, Tensor bias_value, std::vector<int64_t> strides,
                   std::vector<int64_t> pads, std::vector<int64_t> dilations, int64_t group_count)
                : x(std::move(input)), weight(std::move(kernel)), bias(std::move(bias_value)),
                  stride(std::move(strides)), padding(std::move(pads)),
                  dilation(std::move(dilations)), groups(group_count) {}

            std::vector<int64_t> signature() const override {
                int64_t out_ch = weight.shape[0];
                int64_t kernel_height = weight.shape[2], kernel_width = weight.shape[3];
                int64_t H_out =
                    (x.shape[2] + padding[0] + padding[1] - dilation[0] * (kernel_height - 1) - 1) /
                        stride[0] +
                    1;
                int64_t W_out =
                    (x.shape[3] + padding[2] + padding[3] - dilation[1] * (kernel_width - 1) - 1) /
                        stride[1] +
                    1;
                return {x.shape[0], out_ch, H_out, W_out};
            }
    };
}
