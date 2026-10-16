#include "mln/sdk/nn/conv2d.hpp"
#include "mln/sdk/functional/conv2d.hpp"
#include "mln/sdk/operations.hpp"
#include <memory>

namespace Mln::Sdk::Nn {

    Conv2D::Conv2D(int64_t in_ch, int64_t out_ch, int64_t kernel_height, int64_t kernel_width,
                   std::vector<int64_t> stride, std::vector<int64_t> padding,
                   std::vector<int64_t> dilation, int64_t groups, bool use_bias)
        : stride_(stride), padding_(padding), dilation_(dilation), groups_(groups) {
        parameters.insert("weight",
                          Weight{
                              .name = "weight",
                              .shape = {out_ch, in_ch / groups, kernel_height, kernel_width},
                              .value = std::nullopt,
                          });
        parameters.insert("bias", Weight{
                                      .name = "bias",
                                      .shape = {use_bias ? out_ch : 0},
                                      .value = std::nullopt,
                                  });
    }

    Conv2D::Conv2D(int64_t in_ch, int64_t out_ch, int64_t k, int64_t s, int64_t p, int64_t d,
                   int64_t g, bool use_bias)
        : Conv2D(in_ch, out_ch, k, k, {s, s},
                 {p < 0 ? (k - 1) / 2 : p, p < 0 ? (k - 1) / 2 : p, p < 0 ? (k - 1) / 2 : p,
                  p < 0 ? (k - 1) / 2 : p},
                 {d, d}, g, use_bias) {}

    Tensor Conv2D::forward(const Tensor &x) {
        Tensor w = parameters.at("weight");
        Tensor b = parameters.at("bias");
        return operations.insert<Functional::Conv2D>(x, w, b, stride_, padding_, dilation_,
                                                     groups_);
    }

}
