#pragma once

#include "mln/sdk/functional/input.hpp"
#include "mln/sdk/operations.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace Mln::Sdk::Functional {
    class Loop : public Operation::Of<Kind::Loop> {
        public:
            Tensor initial;
            int64_t iterations;
            std::unique_ptr<Operations> body;
            Tensor index_argument;
            Tensor value_argument;
            Tensor result;

            template <typename Build>
            Loop(Tensor value, int64_t count, Build &&build)
                : initial(std::move(value)), iterations(count),
                  body(std::make_unique<Operations>()) {
                index_argument = body->insert<Input>();
                index_argument.dtype = DType::Int32;
                value_argument = body->insert<Input>();
                value_argument.shape = initial.shape;
                value_argument.dtype = initial.dtype;
                result = std::forward<Build>(build)(*body, index_argument, value_argument);
            }

            void set_policy_path(std::string path) { body->set_policy_path(std::move(path)); }

            std::vector<int64_t> signature() const override { return initial.shape; }
    };
}
