#pragma once

#include "mln/dtype.hpp"
#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include "mln/sdk/tensor.hpp"
#include "mln/sdk/weight.hpp"
#include "mln/weight.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Mln::Sdk {
    class Operations;
}

namespace Mln::Sdk {
    class Codec {
        public:
            struct Parameter {
                    struct Component {
                            std::string suffix;
                            std::vector<uint8_t> bytes;
                    };
            };

            enum class Scope {
                Any,
                Constant,
                Input,
                Output,
                Internal,
            };

            enum class Lifetime {
                Any,
                Immutable,
                Invocation,
                Session,
            };

            enum class Update {
                None,
                Whole,
                Region,
            };

            class Format;
            struct Scheme;
            std::string name;

            virtual ~Codec() = default;

            virtual std::string storage_name() const { return name; }

            virtual Tensor decode(Operations &operations, const Weight &weight, DType dtype) = 0;
            virtual Tensor decode(Operations &operations, const Weight &weight,
                                  const Scheme &scheme, DType dtype) {
                (void)scheme;
                return decode(operations, weight, dtype);
            }
            virtual bool supports(const Weight &) const { return true; }
            virtual bool supports(const Weight &weight, const Scheme &scheme) const {
                (void)scheme;
                return supports(weight);
            }
            virtual std::vector<uint8_t> encode(std::span<const float>) const {
                raise(Sdk::Errors::Codec::Data::Encode::Unavailable, name);
            }
            virtual std::vector<uint8_t> encode(std::span<const float> values,
                                                const Scheme &scheme) const {
                (void)scheme;
                return encode(values);
            }
            virtual std::vector<Parameter::Component>
            encode(std::span<const float> values, const Weight &, const Scheme &scheme) const {
                return {{"", encode(values, scheme)}};
            }
            virtual std::vector<float> decode(std::span<const uint8_t>) const {
                raise(Sdk::Errors::Codec::Data::Decode::Unavailable, name);
            }
            virtual std::vector<float> decode(std::span<const uint8_t> bytes,
                                              const Scheme &scheme) const {
                (void)scheme;
                return decode(bytes);
            }
    };

    class Codec::Format {
        private:
            std::shared_ptr<Codec> value_;

        public:
            Format() = default;

            template <typename T>
                requires std::is_base_of_v<Codec, std::remove_cvref_t<T>>
            Format(T &&value)
                : value_(std::make_shared<std::remove_cvref_t<T>>(std::forward<T>(value))) {}

            explicit operator bool() const { return value_ != nullptr; }

            Codec &get() const {
                if (!value_) {
                    raise(Sdk::Errors::Codec::Format::Uninitialized);
                }

                return *value_;
            }
    };

    struct Codec::Scheme {
            Scope scope = Scope::Any;
            Lifetime lifetime = Lifetime::Any;
            Update update = Update::Whole;
            Mln::Weight::Residency residency = Mln::Weight::Residency::Retained;
            Format format;
    };

}
