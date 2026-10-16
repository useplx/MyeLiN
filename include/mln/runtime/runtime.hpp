#pragma once

#include "mln/buffer.hpp"
#include "mln/sdk/driver/module.hpp"
#include "mln/sdk/native/serialization.hpp"

#include <atomic>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Mln::Sdk::Native {
    struct Context;
    template <typename T, typename Enable> struct Argument;
} // namespace Mln::Sdk::Native

namespace Mln {
    class Runtime;
    namespace Wasm {
        struct Symbol;
        std::vector<Symbol> core_symbols(Runtime &runtime);
    } // namespace Wasm
} // namespace Mln

namespace Mln {
    class Runtime {
            struct Impl;

        public:
            struct Errors;

            struct Device {
                    std::string driver;
                    std::string path;
                    std::string name;
            };

            struct Route {
                    std::string device;
                    std::uint64_t invocations = 0;
                    double latency_ms = 0.0;
            };

            Runtime();
            explicit Runtime(const std::filesystem::path &configuration);

            template <typename T, typename Enable = void> struct Argument {
                    using Writer = Sdk::Native::Return<void>::Writer;

                    static void write(Writer &writer, const T &value) {
                        Sdk::Native::Return<T>::write(writer, value);
                    }
            };

            template <typename T, typename Enable = void> struct Return {
                    using Reader = Sdk::Native::Argument<void>::Reader;

                    [[nodiscard]] static T read(Reader &reader) {
                        return Sdk::Native::Argument<T>::read(reader);
                    }
            };

            class Session {
                public:
                    struct Output {
                            using Callback = std::function<void(std::span<const uint8_t>)>;

                            template <typename Signature> struct Value;

                            template <typename Owner, typename Result, typename Argument>
                            struct Value<Result (Owner::*)(Argument) const> {
                                    using Type = std::remove_cvref_t<Argument>;
                            };

                            template <typename Owner, typename Result, typename Argument>
                            struct Value<Result (Owner::*)(Argument)> {
                                    using Type = std::remove_cvref_t<Argument>;
                            };
                    };

                    class State {
                        public:
                            [[nodiscard]] std::size_t size() const { return data_.size(); }
                            [[nodiscard]] bool empty() const { return data_.empty(); }
                            void read(std::span<std::byte> destination) const {
                                data_.read(destination);
                            }
                            void read(std::size_t offset, std::span<std::byte> destination) const {
                                data_.read(offset, destination);
                            }
                            void write(std::span<const std::byte> source) { data_.write(source); }
                            void write(std::size_t offset, std::span<const std::byte> source) {
                                data_.write(offset, source);
                            }
                            [[nodiscard]] Buffer &data() noexcept { return data_; }
                            [[nodiscard]] const Buffer &data() const noexcept { return data_; }

                        private:
                            friend class Session;
                            explicit State(Buffer data) : data_(std::move(data)) {}
                            Buffer data_;
                    };

                    Session(Session &&other) noexcept;
                    Session &operator=(Session &&other) noexcept;
                    Session(const Session &) = delete;
                    Session &operator=(const Session &) = delete;
                    ~Session();

                    void load(std::string_view package_path, std::string_view backend = {});
                    [[nodiscard]] std::vector<std::string> devices() const;
                    [[nodiscard]] std::vector<Route> routes() const;
                    void invoke(const Buffer &input, Output::Callback callback);
                    void invoke(std::string_view entrypoint, const Buffer &input,
                                Output::Callback callback);
                    void cancel() noexcept;

                    template <typename Input, typename Callback>
                        requires(!std::same_as<std::remove_cvref_t<Input>, Buffer> &&
                                 requires { &std::remove_reference_t<Callback>::operator(); })
                    void invoke(std::string_view entrypoint, const Input &input,
                                Callback callback) {
                        using Result = typename Output::Value<
                            decltype(&std::remove_reference_t<Callback>::operator())>::Type;
                        typename Argument<std::remove_cvref_t<Input>>::Writer writer;
                        Argument<std::remove_cvref_t<Input>>::write(writer, input);
                        Buffer argument(writer.size());
                        argument.write(writer.bytes());
                        invoke(entrypoint, argument,
                               [callback = std::move(callback)](
                                   std::span<const std::uint8_t> bytes) mutable {
                                   typename Return<Result>::Reader reader(std::as_bytes(bytes));
                                   Result result = Return<Result>::read(reader);
                                   if (!reader.empty())
                                       Runtime::return_bytes_trailing(reader.remaining());
                                   callback(std::move(result));
                               });
                    }
                    [[nodiscard]] Buffer invoke(std::string_view entrypoint, Buffer input) const;
                    [[nodiscard]] std::vector<Buffer> invoke(std::string_view entrypoint,
                                                             std::span<const Buffer> inputs) const;
                    [[nodiscard]] std::vector<Buffer>
                    invoke(std::string_view entrypoint,
                           std::span<const std::reference_wrapper<const Buffer>> inputs) const;
                    [[nodiscard]] State state(std::string_view name, std::size_t bytes) const;
                    [[nodiscard]] State state(std::string_view name, Buffer value) const;
                    [[nodiscard]] Buffer asset(std::string_view name) const;
                    void emit(const Buffer &value, std::size_t offset = 0) const;
                    [[nodiscard]] static Session borrow(Sdk::Native::Context &context,
                                                        std::uint32_t identifier);

                private:
                    friend class Runtime;
                    template <typename T, typename Enable> friend struct Sdk::Native::Argument;
                    explicit Session(Runtime &runtime);
                    explicit Session(std::uint32_t abi_identifier) noexcept;
                    void reset() noexcept;

                    Runtime *runtime_ = nullptr;
                    std::weak_ptr<const void> lifetime_;
                    uint32_t id_ = 0;
                    bool owning_ = true;
                    std::shared_ptr<std::atomic_bool> cancellation_;
            };

            [[nodiscard]] Session session();
            [[nodiscard]] std::vector<Device> devices() const;

            ~Runtime();
            Runtime(const Runtime &) = delete;
            Runtime &operator=(const Runtime &) = delete;

        private:
            friend std::vector<Wasm::Symbol> Wasm::core_symbols(Runtime &runtime);
            friend Sdk::Native::Context make_native_context(Runtime &runtime);
            [[noreturn]] static void return_bytes_trailing(std::size_t bytes);
            uint32_t load(uint32_t session, const std::string &package_path,
                          std::string_view backend);
            void unload(uint32_t session) noexcept;
            std::vector<std::string> devices(uint32_t session) const;
            std::vector<Route> routes(uint32_t session) const;
            void invoke(uint32_t session, const Buffer &input, Session::Output::Callback callback);
            void invoke(uint32_t session, const std::string &entrypoint, const Buffer &input,
                        Session::Output::Callback callback);
            uint32_t invoke(uint32_t session, uint32_t input);
            uint32_t invoke(uint32_t session, const std::string &entrypoint, uint32_t input);
            uint32_t retain(const Mln::Buffer &buffer);
            uint32_t allocate(uint32_t bytes);
            void release(uint32_t buffer);
            uint32_t length(uint32_t buffer);
            uint8_t byte_at(uint32_t buffer, uint32_t index);
            void write_byte(uint32_t buffer, uint32_t index, uint8_t value);
            void read(uint32_t buffer, uint32_t offset, void *dst, uint32_t len);
            void write(uint32_t buffer, uint32_t offset, const void *src, uint32_t len);
            const void *data(uint32_t buffer) const;
            uint32_t execute(uint32_t session, const std::string &path, uint32_t input);
            std::vector<uint32_t> execute(uint32_t session, const std::string &path,
                                          std::span<const uint32_t> inputs);
            uint32_t state(uint32_t session, const std::string &name, uint32_t bytes);
            uint32_t set_state(uint32_t session, const std::string &name, uint32_t buffer);
            uint32_t asset(uint32_t session, const std::string &name);
            void emit(uint32_t session, uint32_t buffer, uint32_t offset);
            std::unique_ptr<Impl> impl_;
            std::shared_ptr<const void> lifetime_;
    };
} // namespace Mln
