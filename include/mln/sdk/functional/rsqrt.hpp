#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Rsqrt : public Unary<Kind::Rsqrt> {
        public:
            explicit Rsqrt(Tensor input) : Unary<Kind::Rsqrt>(std::move(input)) {}
    };
}
