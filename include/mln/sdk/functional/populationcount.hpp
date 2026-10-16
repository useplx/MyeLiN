#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class PopulationCount : public Unary<Kind::PopulationCount> {
        public:
            explicit PopulationCount(Tensor input)
                : Unary<Kind::PopulationCount>(std::move(input)) {}
    };
}
