#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Input : public Operation::Of<Kind::Input> {
        public:
            Input() {}

            std::vector<int64_t> signature() const override { return {}; }
    };
}
