#pragma once

#include "mln/sdk/operations.hpp"
#include "mln/sdk/tensor.hpp"
#include "mln/sdk/weight.hpp"

#include <map>
#include <string>
#include <string_view>

namespace Mln::Sdk {
    class Module;

    class Parameters {
        private:
            Module *owner;
            std::map<std::string, Weight, std::less<>> parameters;

        public:
            explicit Parameters(Module &owner);

            void insert(std::string name, Weight weight);

            Tensor at(std::string_view name);
    };
}
