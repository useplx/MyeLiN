#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Sqrt : public Unary<Kind::Sqrt> {
        public:
            explicit Sqrt(Tensor input) : Unary<Kind::Sqrt>(std::move(input)) {}
    };
}
