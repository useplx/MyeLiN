#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class DotGeneral : public Operation::Of<Kind::DotGeneral> {
        public:
            Tensor lhs;
            Tensor rhs;
            std::vector<int64_t> lhs_batch;
            std::vector<int64_t> rhs_batch;
            std::vector<int64_t> lhs_contract;
            std::vector<int64_t> rhs_contract;
            std::vector<int64_t> shape;

            DotGeneral(Tensor left, Tensor right, std::vector<int64_t> lhs_batch_dimensions,
                       std::vector<int64_t> rhs_batch_dimensions,
                       std::vector<int64_t> lhs_contracting_dimensions,
                       std::vector<int64_t> rhs_contracting_dimensions,
                       std::vector<int64_t> output_shape)
                : lhs(std::move(left)), rhs(std::move(right)),
                  lhs_batch(std::move(lhs_batch_dimensions)),
                  rhs_batch(std::move(rhs_batch_dimensions)),
                  lhs_contract(std::move(lhs_contracting_dimensions)),
                  rhs_contract(std::move(rhs_contracting_dimensions)),
                  shape(std::move(output_shape)) {}

            std::vector<int64_t> signature() const override { return shape; }
    };
}
