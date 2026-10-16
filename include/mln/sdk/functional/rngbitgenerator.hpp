#pragma once

#include "mln/dtype.hpp"
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    enum class RngAlgorithm { Default, ThreeFry, Philox };

    class RngBitGenerator : public Operation::Of<Kind::RngBitGenerator> {
        public:
            Tensor initial_state;
            std::vector<int64_t> output_shape;
            DType output_dtype;
            RngAlgorithm algorithm;

            RngBitGenerator(Tensor state, std::vector<int64_t> random_shape, DType random_dtype,
                            RngAlgorithm selected_algorithm)
                : initial_state(std::move(state)), output_shape(std::move(random_shape)),
                  output_dtype(random_dtype), algorithm(selected_algorithm) {}

            std::vector<int64_t> signature() const override { return initial_state.shape; }

            std::vector<std::vector<int64_t>> signatures() const override {
                return {initial_state.shape, output_shape};
            }

            std::vector<std::optional<DType>> result_dtypes() const override {
                return {initial_state.dtype, output_dtype};
            }
    };
}
