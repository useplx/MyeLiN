#include <mln/sdk/tensor.hpp>

namespace Mln::Sdk {

    Tensor::Tensor(std::vector<int64_t> dimensions) : shape(std::move(dimensions)) {}
    Tensor::Tensor(std::vector<int64_t> dimensions, DType element_type)
        : shape(std::move(dimensions)), dtype(element_type) {}

}
