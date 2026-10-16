#pragma once

#include <string_view>

namespace Mln::Sdk::Functional {
    enum class Kind {
        Input,
        Const,
        Add,
        Subtract,
        Multiply,
        MatMul,
        Attention,
        Reshape,
        Broadcast,
        Concatenate,
        Convert,
        BitcastConvert,
        BitwiseAnd,
        ShiftRightLogical,
        Iota,
        DynamicIota,
        Dot,
        DotGeneral,

        Conv2D,
        Logistic,
        MaxPool2D,
        Slice,
        Upsample,

        Transpose,
        Exp,
        Log,
        Softplus,
        ReduceSum,
        Reduce,
        Sort,
        Rng,
        RngBitGenerator,
        Scatter,
        Divide,
        Equal,
        Select,

        Sqrt,
        Rsqrt,
        Sin,
        Cos,
        Tanh,
        ShiftLeftLogical,
        ReduceMax,
        Gather,
        DynamicSlice,
        DynamicUpdateSlice,
        Map,
        Loop,
        CausalConv1D,
        GatedDeltaRule,
        Abs,
        Negate,
        Floor,
        Ceil,
        Maximum,
        Minimum,
        Power,
        Remainder,
        BitwiseOr,
        BitwiseXor,
        Cbrt,
        Expm1,
        Log1p,
        Round,
        RoundNearestEven,
        Sign,
        Tan,
        CountLeadingZeros,
        PopulationCount,
        BitwiseNot,
        Atan2,
        ShiftRightArithmetic,
        NotEqual,
        Greater,
        GreaterEqual,
        Less,
        LessEqual,
        IsFinite,
        Clamp,
        Reverse,
        Pad,
        GetDimensionSize,
        SetDimensionSize,
        OptimizationBarrier,
        ReducePrecision,
        DynamicReshape,
        DynamicBroadcast,
        RealDynamicSlice,
        DynamicPad,
        Cholesky,
        TriangularSolve,
        BatchNormGrad,
        BatchNormTraining,
        BatchNormInference,
        Einsum,
        UnaryEinsum,
        Count,
    };

    constexpr std::string_view name(Kind kind) noexcept {
        switch (kind) {
        case Kind::Input:
            return "input";
        case Kind::Const:
            return "const";
        case Kind::Add:
            return "add";
        case Kind::Subtract:
            return "subtract";
        case Kind::Multiply:
            return "multiply";
        case Kind::MatMul:
            return "matmul";
        case Kind::Attention:
            return "attention";
        case Kind::Reshape:
            return "reshape";
        case Kind::Broadcast:
            return "broadcast";
        case Kind::Concatenate:
            return "concatenate";
        case Kind::Convert:
            return "convert";
        case Kind::BitcastConvert:
            return "bitcast-convert";
        case Kind::BitwiseAnd:
            return "bitwise-and";
        case Kind::ShiftRightLogical:
            return "shift-right-logical";
        case Kind::Iota:
            return "iota";
        case Kind::DynamicIota:
            return "dynamic-iota";
        case Kind::Dot:
            return "dot";
        case Kind::DotGeneral:
            return "dot-general";
        case Kind::Conv2D:
            return "conv2d";
        case Kind::Logistic:
            return "logistic";
        case Kind::MaxPool2D:
            return "max-pool2d";
        case Kind::Slice:
            return "slice";
        case Kind::Upsample:
            return "upsample";
        case Kind::Transpose:
            return "transpose";
        case Kind::Exp:
            return "exp";
        case Kind::Log:
            return "log";
        case Kind::Softplus:
            return "softplus";
        case Kind::ReduceSum:
            return "reduce-sum";
        case Kind::Reduce:
            return "reduce";
        case Kind::Sort:
            return "sort";
        case Kind::Rng:
            return "rng";
        case Kind::RngBitGenerator:
            return "rng-bit-generator";
        case Kind::Scatter:
            return "scatter";
        case Kind::Divide:
            return "divide";
        case Kind::Equal:
            return "equal";
        case Kind::Select:
            return "select";
        case Kind::Sqrt:
            return "sqrt";
        case Kind::Rsqrt:
            return "rsqrt";
        case Kind::Sin:
            return "sin";
        case Kind::Cos:
            return "cos";
        case Kind::Tanh:
            return "tanh";
        case Kind::ShiftLeftLogical:
            return "shift-left-logical";
        case Kind::ReduceMax:
            return "reduce-max";
        case Kind::Gather:
            return "gather";
        case Kind::DynamicSlice:
            return "dynamic-slice";
        case Kind::DynamicUpdateSlice:
            return "dynamic-update-slice";
        case Kind::Map:
            return "map";
        case Kind::Loop:
            return "loop";
        case Kind::CausalConv1D:
            return "causal-conv1d";
        case Kind::GatedDeltaRule:
            return "gated-delta-rule";
        case Kind::Abs:
            return "abs";
        case Kind::Negate:
            return "negate";
        case Kind::Floor:
            return "floor";
        case Kind::Ceil:
            return "ceil";
        case Kind::Maximum:
            return "maximum";
        case Kind::Minimum:
            return "minimum";
        case Kind::Power:
            return "power";
        case Kind::Remainder:
            return "remainder";
        case Kind::BitwiseOr:
            return "bitwise-or";
        case Kind::BitwiseXor:
            return "bitwise-xor";
        case Kind::Cbrt:
            return "cbrt";
        case Kind::Expm1:
            return "expm1";
        case Kind::Log1p:
            return "log1p";
        case Kind::Round:
            return "round";
        case Kind::RoundNearestEven:
            return "round-nearest-even";
        case Kind::Sign:
            return "sign";
        case Kind::Tan:
            return "tan";
        case Kind::CountLeadingZeros:
            return "count-leading-zeros";
        case Kind::PopulationCount:
            return "population-count";
        case Kind::BitwiseNot:
            return "bitwise-not";
        case Kind::Atan2:
            return "atan2";
        case Kind::ShiftRightArithmetic:
            return "shift-right-arithmetic";
        case Kind::NotEqual:
            return "not-equal";
        case Kind::Greater:
            return "greater";
        case Kind::GreaterEqual:
            return "greater-equal";
        case Kind::Less:
            return "less";
        case Kind::LessEqual:
            return "less-equal";
        case Kind::IsFinite:
            return "is-finite";
        case Kind::Clamp:
            return "clamp";
        case Kind::Reverse:
            return "reverse";
        case Kind::Pad:
            return "pad";
        case Kind::GetDimensionSize:
            return "get-dimension-size";
        case Kind::SetDimensionSize:
            return "set-dimension-size";
        case Kind::OptimizationBarrier:
            return "optimization-barrier";
        case Kind::ReducePrecision:
            return "reduce-precision";
        case Kind::DynamicReshape:
            return "dynamic-reshape";
        case Kind::DynamicBroadcast:
            return "dynamic-broadcast";
        case Kind::RealDynamicSlice:
            return "real-dynamic-slice";
        case Kind::DynamicPad:
            return "dynamic-pad";
        case Kind::Cholesky:
            return "cholesky";
        case Kind::TriangularSolve:
            return "triangular-solve";
        case Kind::BatchNormGrad:
            return "batch-norm-grad";
        case Kind::BatchNormTraining:
            return "batch-norm-training";
        case Kind::BatchNormInference:
            return "batch-norm-inference";
        case Kind::Einsum:
            return "einsum";
        case Kind::UnaryEinsum:
            return "unary-einsum";
        case Kind::Count:
            return "unknown";
        }
        return "unknown";
    }
}
