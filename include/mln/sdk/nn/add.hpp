#pragma once

#include <mln/sdk/functional/add.hpp>
#include <mln/sdk/module.hpp>
#include <mln/sdk/tensor.hpp>

#include <vector>

namespace Mln::Sdk::Nn {
    class Add : public Module {
        public:
            template <typename... Ts> Tensor forward(const Ts &...tensors) {
                std::vector<Tensor> args{tensors...};

                Tensor result = args[0];

                for (size_t i = 1; i < args.size(); ++i)
                    result = operations.insert<Functional::Add>(result, args[i]);

                return result;
            }
    };
}
