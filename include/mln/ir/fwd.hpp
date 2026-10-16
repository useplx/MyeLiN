#pragma once

#include <string_view>

namespace mlir {
    class DialectRegistry;
    class ModuleOp;
    class Operation;
}

namespace Mln::Ir {
    using Module = ::mlir::ModuleOp;
    using Operation = ::mlir::Operation;
    using DialectRegistry = ::mlir::DialectRegistry;

    namespace Graph {
        inline constexpr std::string_view Format = "stablehlo";
        inline constexpr std::string_view DotGeneral = "stablehlo.dot_general";
    }
}
