#pragma once

#include "mln/buffer.hpp"
#include "mln/buffer_errors.hpp"
#include "mln/sdk/native/abi.hpp"
#include "mln/sdk/native/serialization.hpp"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace Mln::Sdk::Native {
    template <> struct Argument<Mln::Buffer> {
            [[nodiscard]] static Mln::Buffer read(std::uint32_t identifier) {
                return Mln::Buffer::from_abi(identifier, true);
            }

            [[nodiscard]] static Mln::Buffer borrow(std::uint32_t identifier) {
                return Mln::Buffer::from_abi(identifier, false);
            }

            [[nodiscard]] static Mln::Buffer read(Context *, std::uint32_t identifier) {
                return Mln::Buffer::from_abi(identifier, false);
            }

            [[nodiscard]] static Mln::Buffer read(Context *context, std::uint32_t identifier, int) {
                return Mln::Buffer::from_native(*context, identifier);
            }

            [[nodiscard]] static std::uint32_t identifier(const Mln::Buffer &buffer) {
                return buffer.abi_id();
            }
    };

    template <> struct Return<Mln::Buffer> {
            [[nodiscard]] static std::uint32_t write(Mln::Buffer value) {
                return value.release_to_abi();
            }

            static void write(Context *, Call *call, Mln::Buffer value) {
                call->results[0] = value.release_to_abi();
            }

            static void write(Context *context, Call *call, Mln::Buffer value, int) {
                if (const std::uint32_t identifier = value.native_id(*context)) {
                    call->results[0] = identifier;
                    return;
                }
                if (value.size() > std::numeric_limits<std::uint32_t>::max())
                    raise(Mln::Buffer::Errors::Abi::Size::Overflow, value.size());
                const std::uint32_t bytes = static_cast<std::uint32_t>(value.size());
                const std::uint32_t identifier =
                    context->buffer.allocate(context->user_data, bytes);
                if (bytes != 0) {
                    std::vector<std::byte> storage(bytes);
                    value.read(storage);
                    context->buffer.write(context->user_data, identifier, 0, storage.data(), bytes);
                }
                call->results[0] = identifier;
            }
    };
}
