#include "mln/error.hpp"
#include "mln/sdk/module.hpp"
#include "mln/sdk/orchestration.hpp"

#include <memory>
#include <stdexcept>

namespace {
    class First final : public Mln::Sdk::Module {};
    class Second final : public Mln::Sdk::Module {};

    class Alternate final : public Mln::Sdk::Module {
        public:
            std::string entrypoint() const override { return "model.prefill.forward"; }
    };

    class Counted final : public Mln::Sdk::Module {
        public:
            explicit Counted(unsigned &destructions) : destructions_(destructions) {}
            ~Counted() override { ++destructions_; }

        private:
            unsigned &destructions_;
    };

    class Alpha final : public Mln::Sdk::Orchestration {
        public:
            Mln::Buffer forward(Mln::Runtime::Session &, Mln::Buffer input) override {
                return input;
            }
    };

    class Beta final : public Mln::Sdk::Orchestration {
        public:
            Mln::Buffer forward(Mln::Runtime::Session &, Mln::Buffer input) override {
                return input;
            }
    };

    template <typename F> bool rejects(F &&action) {
        try {
            action();
        } catch (const Mln::Diagnostic &) {
            return true;
        }
        return false;
    }
}

int main() {
    First parent;
    parent.name = "model";
    if (parent.entrypoint() != "model.forward")
        return 8;
    parent.modules.insert<First>("typed");
    if (!parent.modules.at<First>("typed"))
        return 1;
    if (parent.modules.at<First>("typed")->entrypoint() != "model.typed.forward")
        return 9;
    if (!rejects([&] { parent.modules.at<Second>("typed"); }))
        return 2;

    parent.modules.insert("erased", std::make_unique<First>());
    if (!rejects([&] { parent.modules.at<First>("erased"); }))
        return 3;

    unsigned destructions = 0;
    {
        First root;
        root.modules.insert<Counted>("child", destructions);
        root.modules.at<Counted>("child")->modules.insert<Counted>("grandchild", destructions);
    }
    if (destructions != 2)
        return 6;

    Mln::Sdk::Orchestrations orchestrations;
    orchestrations.insert<Alpha>("typed");
    if (!orchestrations.at<Alpha>("typed"))
        return 4;
    if (!rejects([&] { orchestrations.at<Beta>("typed"); }))
        return 5;

    orchestrations.insert("erased", std::make_unique<Alpha>());
    if (!rejects([&] { orchestrations.at<Alpha>("erased"); }))
        return 7;

    Alternate alternate;
    alternate.name = "model";
    if (alternate.path() != "model" || alternate.entrypoint() != "model.prefill.forward")
        return 10;
}
