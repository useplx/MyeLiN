#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class Round : public Unary<Kind::Round> {
        public:
            explicit Round(Tensor input) : Unary<Kind::Round>(std::move(input)) {}
    };
}
