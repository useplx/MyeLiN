#pragma once
#include "mln/sdk/module.hpp"
#include "mln/sdk/tensor.hpp"
#include <cstdint>
#include <vector>

namespace Mln::Sdk::Nn {
    class Conv2D : public Module {
        public:
            Conv2D(int64_t in_ch, int64_t out_ch, int64_t kernel_height, int64_t kernel_width,
                   std::vector<int64_t> stride, std::vector<int64_t> padding,
                   std::vector<int64_t> dilation, int64_t groups, bool use_bias = true);

            Conv2D(int64_t in_ch, int64_t out_ch, int64_t k = 1, int64_t s = 1, int64_t p = -1,
                   int64_t d = 1, int64_t g = 1, bool use_bias = true);

            Tensor forward(const Tensor &x);

        private:
            std::vector<int64_t> stride_, padding_, dilation_;
            int64_t groups_;
    };
}
