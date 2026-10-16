#pragma once

#include "mln/error.hpp"

namespace Mln::Sdk::Errors {
    namespace Module {
        inline constexpr Mln::Error Missing{Mln::Range::Sdk + 0, "module '{}' was not found"};
        namespace Type {
            inline constexpr Mln::Error Mismatch{Mln::Range::Sdk + 1,
                                                 "module '{}' has a different type"};
        }
        namespace Graph {
            inline constexpr Mln::Error Unimplemented{
                Mln::Range::Sdk + 2, "module does not implement the graph boundary"};
        }
    }

    namespace Orchestration {
        inline constexpr Mln::Error Missing{Mln::Range::Sdk + 10,
                                            "orchestration '{}' was not found"};
        namespace Type {
            inline constexpr Mln::Error Mismatch{Mln::Range::Sdk + 11,
                                                 "orchestration '{}' has a different type"};
        }
    }

    namespace Parameter {
        inline constexpr Mln::Error Missing{Mln::Range::Sdk + 20, "parameter '{}' was not found"};
    }

    namespace Operation::Operand {
        inline constexpr Mln::Error Missing{Mln::Range::Sdk + 21,
                                            "operation '{}' requires at least one operand"};
    }

    namespace Session {
        namespace Result {
            namespace Capacity {
                inline constexpr Mln::Error Exceeded{
                    Mln::Range::Sdk + 30, "session returned more results than the {} the caller "
                                          "reserved"};
            }
        }
    }

    namespace Codec {
        namespace Data {
            namespace Encode {
                inline constexpr Mln::Error Unavailable{Mln::Range::Sdk + 40,
                                                        "codec '{}' cannot encode on the host"};
            }
            namespace Decode {
                inline constexpr Mln::Error Unavailable{Mln::Range::Sdk + 41,
                                                        "codec '{}' cannot decode on the host"};
            }
        }
        namespace Format {
            inline constexpr Mln::Error Uninitialized{Mln::Range::Sdk + 42,
                                                      "Codec::Format is not initialized"};
        }
    }

    namespace Native::Import {
        namespace Outside {
            inline constexpr Mln::Error Wasm{
                Mln::Range::Sdk + 50, "native symbol '{}.{}' was called outside Wasm execution"};
        }
    }

    namespace Native::Serialization {
        namespace Field {
            namespace Size {
                inline constexpr Mln::Error Overflow{
                    Mln::Range::Sdk + 51, "serialized field size {} exceeds the 32-bit ABI limit"};
            }
            namespace Bytes {
                inline constexpr Mln::Error Trailing{Mln::Range::Sdk + 52,
                                                     "serialized field contains {} trailing bytes"};
            }
        }
        namespace Value {
            inline constexpr Mln::Error Truncated{
                Mln::Range::Sdk + 53, "serialized value requires {} bytes but only {} remain"};
        }
        namespace Vector {
            namespace Size {
                inline constexpr Mln::Error Overflow{
                    Mln::Range::Sdk + 54,
                    "serialized vector element count {} exceeds the 32-bit ABI limit"};
            }
            namespace Storage {
                namespace Size {
                    inline constexpr Mln::Error Invalid{
                        Mln::Range::Sdk + 55,
                        "serialized vector storage size {} is not divisible by element size {}"};
                }
            }
        }
    }
}
