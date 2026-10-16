#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {

    class Logistic : public Unary<Kind::Logistic> {
        public:
            explicit Logistic(Tensor input) : Unary<Kind::Logistic>(std::move(input)) {}
    };
}
