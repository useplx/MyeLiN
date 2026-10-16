#include "mln/sdk/nn/silu.hpp"
#include "mln/sdk/functional/logistic.hpp"
#include "mln/sdk/functional/multiply.hpp"
#include "mln/sdk/operations.hpp"
#include <memory>

namespace Mln::Sdk::Nn {
    Tensor SiLU::forward(const Tensor &x) {
        Tensor sig = operations.insert<Functional::Logistic>(x);
        return operations.insert<Functional::Multiply>(x, sig);
    }
}
