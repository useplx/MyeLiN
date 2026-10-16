#pragma once

#include "mln/ir/fwd.hpp"
#include "mln/sdk/backend/compatibility.hpp"

#include <span>
#include <string_view>

namespace Mln::Sdk::Lowering {
    class Compatibility {
        public:
            struct Context {
                    using Outline = bool (*)(Ir::Module, std::span<Ir::Operation *const>,
                                             std::string_view);

                    const Backend &backend;
                    std::string_view source;
                    std::string_view target;
                    std::span<Ir::Operation *const> operations;
                    Outline outline;
            };

            virtual ~Compatibility() = default;

            [[nodiscard]] virtual std::string_view source() const noexcept = 0;
            [[nodiscard]] virtual std::string_view target() const noexcept = 0;
            [[nodiscard]] virtual int priority() const noexcept { return 0; }

            [[nodiscard]] virtual bool applies(const Backend &backend) const = 0;

            virtual void register_dialects(Ir::DialectRegistry &) const {}

            [[nodiscard]] virtual bool lower(Ir::Module module, const Context &context) const = 0;
    };
}
