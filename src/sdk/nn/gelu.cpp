#include "mln/sdk/nn/gelu.hpp"
#include "mln/sdk/functional/add.hpp"
#include "mln/sdk/functional/broadcast.hpp"
#include "mln/sdk/functional/const.hpp"
#include "mln/sdk/functional/multiply.hpp"
#include "mln/sdk/functional/tanh.hpp"
#include "mln/sdk/operations.hpp"

#include <vector>

namespace Mln::Sdk::Nn {
    Tensor GELU::forward(const Tensor &x) {
        auto scalar = [&](double value) {
            Tensor constant = operations.insert<Functional::Const>(Weight{"", {}, value}, true);
            return operations.insert<Functional::Broadcast>(std::move(constant), x.shape,
                                                            std::vector<int64_t>{}, x);
        };

        Tensor cubic = operations.insert<Functional::Multiply>(
            x, operations.insert<Functional::Multiply>(x, x));
        Tensor inner = operations.insert<Functional::Add>(
            x, operations.insert<Functional::Multiply>(scalar(0.044715), std::move(cubic)));
        Tensor scaled =
            operations.insert<Functional::Multiply>(scalar(0.7978845608028654), std::move(inner));
        Tensor activated = operations.insert<Functional::Add>(
            scalar(1.0), operations.insert<Functional::Tanh>(std::move(scaled)));

        return operations.insert<Functional::Multiply>(
            operations.insert<Functional::Multiply>(scalar(0.5), x), std::move(activated));
    }
}
