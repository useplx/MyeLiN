#include "mln/sdk/native/entrypoint.hpp"

namespace {
    class Echo final : public Mln::Sdk::Orchestration {
        public:
            Mln::Buffer forward(Mln::Runtime::Session &, Mln::Buffer input) override {
                return input;
            }
    };
} // namespace

ENTRYPOINT(Echo)
