#include "mln/buffer.hpp"
#include "mln/buffer_errors.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace {
    bool accepts_reference(const Mln::Buffer &buffer) { return buffer.size() == 4; }
}

int main() {
    static_assert(!std::is_copy_constructible_v<Mln::Buffer>);
    static_assert(!std::is_copy_assignable_v<Mln::Buffer>);
    static_assert(std::is_nothrow_move_constructible_v<Mln::Buffer>);
    static_assert(std::is_nothrow_move_assignable_v<Mln::Buffer>);

    Mln::Buffer original(4);
    const std::array input{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    original.write(input);
    if (!accepts_reference(original))
        return 1;

    Mln::Buffer *pointer = &original;
    if (pointer->byte_at(2) != 3)
        return 2;

    Mln::Buffer moved = std::move(original);
    if (!original.empty() || moved.byte_at(0) != 1)
        return 3;

    Mln::Buffer copy = moved.clone();
    copy.write_byte(0, 9);
    if (copy.byte_at(0) != 9 || moved.byte_at(0) != 1)
        return 4;

    Mln::Buffer shaped(24, {2, 3, 4});
    if (shaped.shape().size() != 3 || shaped.shape()[0] != 2 || shaped.shape()[1] != 3 ||
        shaped.shape()[2] != 4)
        return 7;
    Mln::Buffer shaped_copy = shaped.clone();
    if (shaped_copy.shape().size() != 3 || shaped_copy.shape()[0] != 2 ||
        shaped_copy.shape()[1] != 3 || shaped_copy.shape()[2] != 4)
        return 8;

    try {
        std::array<std::byte, 5> oversized{};
        moved.read(oversized);
        return 5;
    } catch (const Mln::Diagnostic &diagnostic) {
        if (diagnostic.error() != Mln::Buffer::Errors::Read::Bounds::Exceeded)
            return 6;
    }

    return 0;
}
