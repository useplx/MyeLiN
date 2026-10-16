#include "mln/buffer_errors.hpp"
#include "mln/runtime/runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

struct Token {
        std::uint32_t id;
        std::string text;
};

template <> void Mln::Runtime::Argument<Token>::write(Writer &writer, const Token &token) {
    writer.write_field(token.id);
    writer.write_field(token.text);
}

template <> Token Mln::Runtime::Return<Token>::read(Reader &reader) {
    return {
        .id = reader.read_field<std::uint32_t>(),
        .text = reader.read_field<std::string>(),
    };
}

int main() {
    using Runtime = Mln::Runtime;

    static_assert(std::is_default_constructible_v<Runtime>);
    static_assert(!std::is_copy_constructible_v<Runtime>);
    static_assert(!std::is_copy_assignable_v<Runtime>);

    Runtime first;
    Runtime second;

    auto first_session = first.session();
    auto second_session = first.session();
    auto other_runtime_session = second.session();

    static_assert(!std::is_copy_constructible_v<Runtime::Session>);
    static_assert(std::is_move_constructible_v<Runtime::Session>);
    auto moved_session = std::move(first_session);
    (void)other_runtime_session;
    (void)moved_session;

    auto first_state = moved_session.state("shared-name", 1);
    auto second_state = second_session.state("shared-name", 1);
    const std::array changed{std::byte{73}};
    first_state.write(changed);
    std::array<std::byte, 1> first_value{};
    std::array<std::byte, 1> second_value{};
    first_state.read(first_value);
    second_state.read(second_value);
    if (first_value.front() != changed.front() || second_value.front() != std::byte{})
        return 2;

    {
        auto state_session = first.session();
        auto state = state_session.state("cache", 64);
        if (state.size() != 64)
            return 3;
    }

    Runtime::Argument<Token>::Writer writer;
    Runtime::Argument<Token>::write(writer, Token{.id = 42, .text = "generated"});
    Runtime::Return<Token>::Reader reader(writer.bytes());
    const Token token = Runtime::Return<Token>::read(reader);
    if (token.id != 42 || token.text != "generated" || !reader.empty())
        return 4;

    Mln::Buffer escaped;
    {
        Runtime temporary;
        auto session = temporary.session();
        auto state = session.state("escaped", 8);
        escaped = std::move(state.data());
    }
    try {
        (void)escaped.size();
        return 5;
    } catch (const Mln::Diagnostic &diagnostic) {
        if (diagnostic.error() != Mln::Buffer::Errors::Runtime::Lifetime::Ended)
            return 6;
    }

    return 0;
}
