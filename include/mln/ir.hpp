#pragma once

#include "mln/ir/fwd.hpp"

#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/DialectRegistry.h>
#include <mlir/IR/Operation.h>

namespace Mln::Ir {
    using StringAttribute = ::mlir::StringAttr;
}
