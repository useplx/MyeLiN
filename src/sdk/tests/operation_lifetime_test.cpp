#include <mln/sdk/functional.hpp>
#include <mln/sdk/operations.hpp>

#include <memory>

namespace {
    Mln::Sdk::Tensor build_expression(std::weak_ptr<Mln::Sdk::Operation> &observed) {
        Mln::Sdk::Operations operations;
        Mln::Sdk::Tensor left = operations.insert<Mln::Sdk::Functional::Const>(
            Mln::Sdk::Weight{"", {}, 2.0}, false, Mln::DType::Float32);
        observed = left.def;
        Mln::Sdk::Tensor right = operations.insert<Mln::Sdk::Functional::Const>(
            Mln::Sdk::Weight{"", {}, 3.0}, false, Mln::DType::Float32);
        return operations.insert<Mln::Sdk::Functional::Add>(std::move(left), std::move(right));
    }
}

int main() {
    std::weak_ptr<Mln::Sdk::Operation> observed;
    {
        Mln::Sdk::Tensor expression = build_expression(observed);
        if (!expression.def || observed.expired())
            return 1;
        if (expression.def->kind != Mln::Sdk::Functional::Kind::Add)
            return 2;
    }
    return observed.expired() ? 0 : 3;
}
