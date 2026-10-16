#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class Cbrt : public Unary<Kind::Cbrt> {
        public:
            explicit Cbrt(Tensor input) : Unary<Kind::Cbrt>(std::move(input)) {}
    };
}
