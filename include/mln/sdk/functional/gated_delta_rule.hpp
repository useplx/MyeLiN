#pragma once

#include "mln/sdk/operation.hpp"

#include <utility>

namespace Mln::Sdk::Functional {
    class GatedDeltaRule : public Operation::Of<Kind::GatedDeltaRule> {
        public:
            Tensor query;
            Tensor key;
            Tensor value;
            Tensor decay;
            Tensor beta;
            Tensor initial_state;
            double normalization_epsilon;

            GatedDeltaRule(Tensor query_value, Tensor key_value, Tensor value_value,
                           Tensor decay_value, Tensor beta_value, Tensor state_value,
                           double epsilon = 1.0e-6)
                : query(std::move(query_value)), key(std::move(key_value)),
                  value(std::move(value_value)), decay(std::move(decay_value)),
                  beta(std::move(beta_value)), initial_state(std::move(state_value)),
                  normalization_epsilon(epsilon) {}

            std::vector<int64_t> signature() const override {
                const int64_t output_elements = query.shape[0] * query.shape[1] * query.shape[2];
                const int64_t state_elements =
                    initial_state.shape[0] * initial_state.shape[1] * initial_state.shape[2];
                return {output_elements + state_elements};
            }
    };
}
