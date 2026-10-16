#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class RoundNearestEven : public Unary<Kind::RoundNearestEven> {
        public:
            explicit RoundNearestEven(Tensor input)
                : Unary<Kind::RoundNearestEven>(std::move(input)) {}
    };
}
