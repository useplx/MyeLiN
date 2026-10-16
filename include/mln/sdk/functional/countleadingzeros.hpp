#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class CountLeadingZeros : public Unary<Kind::CountLeadingZeros> {
        public:
            explicit CountLeadingZeros(Tensor input)
                : Unary<Kind::CountLeadingZeros>(std::move(input)) {}
    };
}
