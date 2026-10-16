#pragma once

#include "mln/error.hpp"

namespace Mln::Package::Errors {
    namespace File {
        inline constexpr Error Unopenable{Range::Package + 0, "Package: could not open '{}': {}"};
        namespace Stat {
            inline constexpr Error Failed{Range::Package + 1, "Package: fstat failed: {}"};
        }
        namespace Map {
            inline constexpr Error Failed{Range::Package + 2, "Package: mmap failed: {}"};
            namespace Discard {
                inline constexpr Error Failed{Range::Package + 6, "Package: madvise failed: {}"};
                inline constexpr Error Outside{Range::Package + 7,
                                               "Package: discard range is outside the mapping"};
            }
        }
        namespace Not {
            inline constexpr Error Creatable{Range::Package + 3, "Package: could not create {}"};
        }
        namespace Write {
            inline constexpr Error Failed{Range::Package + 4, "Package: failed to write {}"};
        }
        namespace Too {
            inline constexpr Error Large{Range::Package + 5, "Package: file is too large"};
        }
    }

    namespace Header {
        inline constexpr Error Truncated{Range::Package + 10,
                                         "Package: file is smaller than its header"};
        namespace Magic {
            inline constexpr Error Mismatch{Range::Package + 11,
                                            "Package: incompatible magic or version"};
        }
        namespace Size {
            inline constexpr Error Mismatch{Range::Package + 12,
                                            "Package: declared size does not match the file"};
        }
        namespace Alignment {
            inline constexpr Error Overflow{Range::Package + 13, "Package: alignment overflow"};
        }
    }

    namespace Section {
        inline constexpr Error Invalid{Range::Package + 22, "Package: invalid section at index {}"};
        inline constexpr Error Overlapping{Range::Package + 23, "Package: overlapping sections"};
        inline constexpr Error Duplicated{Range::Package + 24, "Package: duplicate section: {}"};
        inline constexpr Error Missing{Range::Package + 26,
                                       "Package: required section is missing: {}"};
        inline constexpr Error Empty{Range::Package + 28, "Package: empty section: {}"};
        inline constexpr Error None{Range::Package + 29, "Package: no sections to write"};
        namespace Count {
            inline constexpr Error Invalid{Range::Package + 20, "Package: invalid section count"};
        }
        namespace Table {
            inline constexpr Error Invalid{Range::Package + 21, "Package: invalid section table"};
        }
        namespace Not {
            inline constexpr Error Unique{Range::Package + 25,
                                          "Package: expected exactly one section of type {}"};
        }
        namespace Name::Too {
            inline constexpr Error Long{Range::Package + 27,
                                        "Package: section name exceeds 48 bytes"};
        }
        namespace Too {
            inline constexpr Error Many{Range::Package + 30, "Package: too many sections"};
        }
    }
}
