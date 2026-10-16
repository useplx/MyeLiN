#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    enum class SolveTranspose { None, Transpose, Adjoint };

    class TriangularSolve : public Operation::Of<Kind::TriangularSolve> {
        public:
            Tensor a;
            Tensor b;
            bool left_side;
            bool lower;
            bool unit_diagonal;
            SolveTranspose transpose_a;

            TriangularSolve(Tensor coefficients, Tensor rhs, bool solve_left = true,
                            bool use_lower = false, bool diagonal_is_one = false,
                            SolveTranspose transpose = SolveTranspose::None)
                : a(std::move(coefficients)), b(std::move(rhs)), left_side(solve_left),
                  lower(use_lower), unit_diagonal(diagonal_is_one), transpose_a(transpose) {}

            std::vector<int64_t> signature() const override { return b.shape; }
    };
}
