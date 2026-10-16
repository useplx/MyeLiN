#pragma once

#include "mln/buffer.hpp"
#include "mln/runtime/runtime.hpp"
#include "mln/sdk/modules.hpp"
#include "mln/sdk/orchestrations.hpp"

#include <string>

namespace Mln::Sdk {
    class Orchestration {
        public:
            std::string name;
            Modules modules;
            Orchestrations orchestrations;

            Orchestration() : modules(nullptr) {}
            virtual ~Orchestration() = default;

            virtual Buffer forward(Runtime::Session &session, Buffer input) = 0;
    };
}
