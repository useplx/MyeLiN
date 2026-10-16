#pragma once

#include "mln/error.hpp"

namespace Mln::Weight::Errors {
    namespace Index {
        namespace String::Too {
            inline constexpr Mln::Error Large{Mln::Range::Weights + 0,
                                              "WeightIndex: string is too large"};
        }
        inline constexpr Mln::Error Truncated{Mln::Range::Weights + 1,
                                              "WeightIndex: truncated data"};
        namespace Too::Many {
            inline constexpr Mln::Error Entries{Mln::Range::Weights + 2,
                                                "WeightIndex: too many entries"};
        }
        namespace Magic {
            inline constexpr Mln::Error Mismatch{Mln::Range::Weights + 3,
                                                 "WeightIndex: incompatible magic"};
        }
        namespace Kind {
            inline constexpr Mln::Error Mismatch{Mln::Range::Weights + 4,
                                                 "WeightIndex: incompatible kind"};
        }
        namespace Trailing {
            inline constexpr Mln::Error Bytes{Mln::Range::Weights + 5,
                                              "WeightIndex: trailing bytes"};
        }
    }

    namespace Descriptor {
        inline constexpr Mln::Error Incomplete{Mln::Range::Weights + 10,
                                               "WeightIndex: incomplete descriptor: {}"};
        namespace Too {
            inline constexpr Mln::Error Large{Mln::Range::Weights + 11,
                                              "WeightIndex: descriptor is too large: {}"};
        }
        inline constexpr Mln::Error Invalid{Mln::Range::Weights + 12,
                                            "WeightIndex: invalid descriptor"};

        namespace Dimension {
            inline constexpr Mln::Error Invalid{Mln::Range::Weights + 13,
                                                "WeightIndex: invalid dimension: {}"};
        }
        namespace Not {
            inline constexpr Mln::Error Single{
                Mln::Range::Weights + 14,
                "IRPA: metadata must contain exactly one weight descriptor"};
        }
    }

    inline constexpr Mln::Error Duplicated{Mln::Range::Weights + 20,
                                           "WeightIndex: duplicate weight: {}"};
    inline constexpr Mln::Error Missing{Mln::Range::Weights + 21, "missing weight: '{}'"};
    namespace Shape {
        inline constexpr Mln::Error Mismatch{Mln::Range::Weights + 22,
                                             "weight '{}': incompatible shape"};
    }
    namespace Storage {
        inline constexpr Mln::Error Empty{Mln::Range::Weights + 23, "weight '{}': empty storage"};
    }
    namespace Quant {
        inline constexpr Mln::Error Mismatch{
            Mln::Range::Weights + 24,
            "weight '{}': incompatible Quant; graph requires {}, but file contains {}"};
    }
    namespace Index {
        inline constexpr Mln::Error Tampered{
            Mln::Range::Weights + 25,
            "weight '{}': tampered index; Quant {} does not match storage {}"};
    }

    namespace File {
        inline constexpr Mln::Error Unopenable{Mln::Range::Weights + 30, "could not open {}"};
        namespace Size {
            inline constexpr Mln::Error Invalid{Mln::Range::Weights + 31, "invalid size in {}"};
        }
        namespace Read {
            inline constexpr Mln::Error Failed{Mln::Range::Weights + 32, "failed to read {}"};
        }
        namespace Write {
            inline constexpr Mln::Error Failed{Mln::Range::Weights + 33, "failed to write {}"};
        }
    }
}
