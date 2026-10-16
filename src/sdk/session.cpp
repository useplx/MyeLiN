#include "mln/abi/abi.hpp"
#include "mln/runtime/runtime.hpp"
#include "mln/sdk/native/buffer.hpp"

#include <array>
#include <limits>
#include <string>
#include <utility>

#if defined(__wasm__)
namespace Mln {
    Runtime::Session::Session(std::uint32_t abi_identifier) noexcept : id_(abi_identifier) {}

    Runtime::Session::Session(Session &&other) noexcept
        : runtime_(std::exchange(other.runtime_, nullptr)), lifetime_(std::move(other.lifetime_)),
          id_(std::exchange(other.id_, 0)), owning_(std::exchange(other.owning_, false)) {}

    Runtime::Session &Runtime::Session::operator=(Session &&other) noexcept {
        if (this != &other) {
            reset();
            runtime_ = std::exchange(other.runtime_, nullptr);
            lifetime_ = std::move(other.lifetime_);
            id_ = std::exchange(other.id_, 0);
            owning_ = std::exchange(other.owning_, false);
        }
        return *this;
    }

    Runtime::Session::~Session() { reset(); }

    void Runtime::Session::reset() noexcept {
        runtime_ = nullptr;
        lifetime_.reset();
        id_ = 0;
        owning_ = false;
    }

    Buffer Runtime::Session::invoke(std::string_view entrypoint, Buffer input) const {
        const std::string name(entrypoint);
        const std::uint32_t input_id = Sdk::Native::Return<Buffer>::write(std::move(input));
        return Sdk::Native::Argument<Buffer>::read(
            Sdk::Abi::Session::invoke(id_, name.c_str(), input_id));
    }

    std::vector<Buffer> Runtime::Session::invoke(std::string_view entrypoint,
                                                 std::span<const Buffer> inputs) const {
        std::vector<std::reference_wrapper<const Buffer>> references;
        references.reserve(inputs.size());
        for (const Buffer &input : inputs)
            references.emplace_back(input);
        return invoke(entrypoint, references);
    }

    std::vector<Buffer>
    Runtime::Session::invoke(std::string_view entrypoint,
                             std::span<const std::reference_wrapper<const Buffer>> inputs) const {
        const std::string name(entrypoint);
        constexpr std::uint32_t capacity = 16;
        if (inputs.size() > std::numeric_limits<std::uint32_t>::max())
            __builtin_trap();
        std::vector<std::uint32_t> input_ids;
        input_ids.reserve(inputs.size());
        for (const auto input : inputs)
            input_ids.push_back(Sdk::Native::Argument<Buffer>::identifier(input.get()));
        std::array<std::uint32_t, capacity> output_ids{};
        const std::uint32_t count = Sdk::Abi::Session::invoke(
            id_, name.c_str(),
            static_cast<Sdk::Abi::const_ptr_t>(reinterpret_cast<std::uintptr_t>(input_ids.data())),
            static_cast<std::uint32_t>(input_ids.size()),
            static_cast<Sdk::Abi::ptr_t>(reinterpret_cast<std::uintptr_t>(output_ids.data())),
            capacity);
        if (count > capacity)
            __builtin_trap();
        std::vector<Buffer> outputs;
        outputs.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
            outputs.push_back(Sdk::Native::Argument<Buffer>::read(output_ids[index]));
        return outputs;
    }

    Runtime::Session::State Runtime::Session::state(std::string_view name,
                                                    std::size_t bytes) const {
        const std::string state_name(name);
        if (bytes > std::numeric_limits<std::uint32_t>::max())
            __builtin_trap();
        return State(Sdk::Native::Argument<Buffer>::borrow(
            Sdk::Abi::Session::state(id_, state_name.c_str(), static_cast<std::uint32_t>(bytes))));
    }

    Runtime::Session::State Runtime::Session::state(std::string_view name, Buffer value) const {
        const std::string state_name(name);
        const std::uint32_t identifier = Sdk::Native::Return<Buffer>::write(std::move(value));
        return State(Sdk::Native::Argument<Buffer>::borrow(
            Sdk::Abi::Session::set_state(id_, state_name.c_str(), identifier)));
    }

    Buffer Runtime::Session::asset(std::string_view name) const {
        const std::string asset_name(name);
        return Sdk::Native::Argument<Buffer>::borrow(
            Sdk::Abi::Session::asset(id_, asset_name.c_str()));
    }

    void Runtime::Session::emit(const Buffer &value, std::size_t offset) const {
        if (offset > std::numeric_limits<std::uint32_t>::max())
            __builtin_trap();
        Sdk::Abi::Session::emit(id_, Sdk::Native::Argument<Buffer>::identifier(value),
                                static_cast<std::uint32_t>(offset));
    }
}
#endif
