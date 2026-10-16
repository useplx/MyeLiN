#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class Log1p : public Unary<Kind::Log1p> {
        public:
            explicit Log1p(Tensor input) : Unary<Kind::Log1p>(std::move(input)) {}
    };
}
