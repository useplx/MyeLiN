#include <memory>
#include <mln/sdk/functional/add.hpp>
#include <mln/sdk/functional/matmul.hpp>
#include <mln/sdk/nn/linear.hpp>
#include <mln/sdk/operations.hpp>
#include <vector>

namespace Mln::Sdk::Nn {
    Linear::Linear(Weight weight, Weight bias) {
        parameters.insert("weight", weight);

        parameters.insert("bias", bias);
    }

    Tensor Linear::forward(const Tensor &x) {
        Tensor w = parameters.at("weight");
        Tensor b = parameters.at("bias");

        Tensor y = operations.insert<Functional::MatMul>(x, w);
        y = operations.insert<Functional::Add>(y, b);

        return y;
    }
}
