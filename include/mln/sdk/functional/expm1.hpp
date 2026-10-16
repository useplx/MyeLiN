#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class Expm1 : public Unary<Kind::Expm1> {
        public:
            explicit Expm1(Tensor input) : Unary<Kind::Expm1>(std::move(input)) {}
    };
}
