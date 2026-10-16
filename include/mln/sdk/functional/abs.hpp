#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Abs : public Unary<Kind::Abs> {
        public:
            explicit Abs(Tensor input) : Unary<Kind::Abs>(std::move(input)) {}
    };
}
