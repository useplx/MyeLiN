#pragma once
#include "mln/dtype.hpp"
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Convert : public Operation::Of<Kind::Convert> {
        public:
            Tensor x;
            DType target;

            Convert(Tensor input, DType element_type) : x(std::move(input)), target(element_type) {}

            std::vector<int64_t> signature() const override { return x.shape; }
    };
}
