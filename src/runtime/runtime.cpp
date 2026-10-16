#include "mln/runtime/runtime.hpp"
#include "configuration.hpp"
#include "cuda_context.hpp"
#include "driver_registry.hpp"
#include "mln/error.hpp"
#include "mln/package.hpp"
#include "mln/runtime/metadata.hpp"
#include "mln/sdk/native.hpp"
#include "mln/sdk/native/symbol.hpp"
#include "mln/weight.hpp"
#include "native_registry.hpp"
#include "symbols.hpp"
#include "vm.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "iree/async/frontier_tracker.h"
#include "iree/async/util/proactor_pool.h"
#include "iree/base/threading/numa.h"
#include "iree/hal/buffer_transfer.h"
#include "iree/hal/driver_registry.h"
#include "iree/hal/drivers/local_task/task_driver.h"
#include "iree/hal/local/loaders/registration/init.h"
#include "iree/hal/local/plugins/registration/init.h"
#include "iree/hal/utils/profile_file.h"
#include "iree/io/file_handle.h"
#include "iree/io/formats/irpa/irpa_parser.h"
#include "iree/io/formats/parser_registry.h"
#include "iree/io/formats/safetensors/safetensors_parser.h"
#include "iree/io/parameter_index.h"
#include "iree/io/parameter_index_provider.h"
#include "iree/modules/hal/module.h"
#include "iree/modules/io/parameters/module.h"
#include "iree/runtime/api.h"
#include "iree/task/api.h"
#include "iree/vm/bytecode/module.h"

namespace Mln {
    namespace {
        namespace Scope {
            template <typename Cleanup> class Guard {
                public:
                    explicit Guard(Cleanup cleanup) : cleanup_(std::move(cleanup)) {}
                    ~Guard() noexcept {
                        if (!active_)
                            return;
                        try {
                            cleanup_();
                        } catch (...) {
                        }
                    }

                    Guard(const Guard &) = delete;
                    Guard &operator=(const Guard &) = delete;

                    void dismiss() noexcept { active_ = false; }

                private:
                    Cleanup cleanup_;
                    bool active_ = true;
            };
        } // namespace Scope

        void check(iree_status_t status, const Mln::Error &error);

        namespace Iree {
            template <typename T, void (*Release)(T *)> struct Deleter {
                    void operator()(T *value) const noexcept { Release(value); }
            };

            namespace File {
                using Handle =
                    std::unique_ptr<iree_io_file_handle_t,
                                    Deleter<iree_io_file_handle_t, iree_io_file_handle_release>>;
            }
            namespace Index {
                using Handle = std::unique_ptr<
                    iree_io_parameter_index_t,
                    Deleter<iree_io_parameter_index_t, iree_io_parameter_index_release>>;
            }
            namespace Provider {
                using Handle = std::unique_ptr<
                    iree_io_parameter_provider_t,
                    Deleter<iree_io_parameter_provider_t, iree_io_parameter_provider_release>>;
            }
            namespace Module {
                using Handle = std::unique_ptr<iree_vm_module_t,
                                               Deleter<iree_vm_module_t, iree_vm_module_release>>;
            }
            namespace Instance {
                using Handle = std::unique_ptr<
                    iree_runtime_instance_t,
                    Deleter<iree_runtime_instance_t, iree_runtime_instance_release>>;
            }
            namespace Device {
                using Handle = std::unique_ptr<iree_hal_device_t,
                                               Deleter<iree_hal_device_t, iree_hal_device_release>>;
            }
            namespace Buffer {
                using Handle = std::unique_ptr<iree_hal_buffer_t,
                                               Deleter<iree_hal_buffer_t, iree_hal_buffer_release>>;
            }
            namespace Device::Group {
                using Handle = std::unique_ptr<
                    iree_hal_device_group_t,
                    Deleter<iree_hal_device_group_t, iree_hal_device_group_release>>;
            }
            namespace Profile {
                using Sink = std::unique_ptr<
                    iree_hal_profile_sink_t,
                    Deleter<iree_hal_profile_sink_t, iree_hal_profile_sink_release>>;

                class Session {
                    public:
                        static std::unique_ptr<Session>
                        create(std::span<const Iree::Device::Handle> devices, std::string output,
                               bool device_timing) {
                            if (output.empty())
                                return nullptr;

                            static std::atomic_uint64_t next_capture{1};
                            const std::string identifier = std::to_string(
                                next_capture.fetch_add(1, std::memory_order_relaxed));
                            const std::size_t placeholder = output.find("{capture}");
                            if (placeholder != std::string::npos)
                                output.replace(placeholder, 9, identifier);
                            else
                                output += "." + identifier;

                            iree_io_file_handle_t *file_raw = nullptr;
                            check(iree_io_file_handle_create(
                                      IREE_IO_FILE_MODE_WRITE | IREE_IO_FILE_MODE_SEQUENTIAL_SCAN |
                                          IREE_IO_FILE_MODE_SHARE_READ,
                                      iree_make_string_view(output.data(), output.size()),
                                      /*initial_size=*/0, iree_allocator_system(), &file_raw),
                                  Runtime::Errors::Iree::File::Wrap::Failed);
                            Iree::File::Handle file(file_raw);

                            iree_hal_profile_sink_t *sink_raw = nullptr;
                            check(iree_hal_profile_file_sink_create(
                                      file.get(), iree_allocator_system(), &sink_raw),
                                  Runtime::Errors::Iree::Session::Create::Failed);
                            auto profile = std::unique_ptr<Session>(new Session(Sink(sink_raw)));

                            iree_hal_device_profiling_options_t options{};
                            options.data_families =
                                IREE_HAL_DEVICE_PROFILING_DATA_QUEUE_EVENTS |
                                IREE_HAL_DEVICE_PROFILING_DATA_EXECUTABLE_METADATA |
                                IREE_HAL_DEVICE_PROFILING_DATA_MEMORY_EVENTS;
                            if (device_timing) {
                                options.data_families |=
                                    IREE_HAL_DEVICE_PROFILING_DATA_DEVICE_QUEUE_EVENTS |
                                    IREE_HAL_DEVICE_PROFILING_DATA_DISPATCH_EVENTS;
                            }
                            options.sink = profile->sink_.get();
                            for (const Iree::Device::Handle &device : devices) {
                                try {
                                    check(iree_hal_device_profiling_begin(device.get(), &options),
                                          Runtime::Errors::Iree::Session::Create::Failed);
                                } catch (...) {
                                    profile->finish();
                                    throw;
                                }
                                profile->devices_.push_back(device.get());
                            }
                            return profile;
                        }

                        ~Session() noexcept { finish(); }

                        Session(const Session &) = delete;
                        Session &operator=(const Session &) = delete;

                    private:
                        explicit Session(Sink sink) : sink_(std::move(sink)) {}

                        void finish() noexcept {
                            for (auto device = devices_.rbegin(); device != devices_.rend();
                                 ++device)
                                iree_status_ignore(iree_hal_device_profiling_end(*device));
                            devices_.clear();
                        }

                        Sink sink_;
                        std::vector<iree_hal_device_t *> devices_;
                };
            } // namespace Profile
            namespace Frontier {
                using Handle = std::unique_ptr<
                    iree_async_frontier_tracker_t,
                    Deleter<iree_async_frontier_tracker_t, iree_async_frontier_tracker_release>>;
            }
            namespace Context {
                using Handle = std::unique_ptr<iree_vm_context_t,
                                               Deleter<iree_vm_context_t, iree_vm_context_release>>;
            }
            namespace List {
                using Handle =
                    std::unique_ptr<iree_vm_list_t, Deleter<iree_vm_list_t, iree_vm_list_release>>;
            }
            namespace View {
                using Handle =
                    std::unique_ptr<iree_hal_buffer_view_t,
                                    Deleter<iree_hal_buffer_view_t, iree_hal_buffer_view_release>>;
            }
            namespace Proactor::Pool {
                using Handle = std::unique_ptr<
                    iree_async_proactor_pool_t,
                    Deleter<iree_async_proactor_pool_t, iree_async_proactor_pool_release>>;
            }
            class Session {
                public:
                    explicit Session(Context::Handle context) : context_(std::move(context)) {}

                    [[nodiscard]] iree_vm_context_t *context() const noexcept {
                        return context_.get();
                    }

                    void append(iree_vm_module_t *module) {
                        check(iree_vm_context_register_modules(context_.get(), 1, &module),
                              Runtime::Errors::Iree::Session::Module::Append::Failed);
                    }

                private:
                    Context::Handle context_;
            };

            class Call {
                public:
                    Call(Session &session, std::string_view name) : session_(session) {
                        check(iree_vm_context_resolve_function(
                                  session.context(),
                                  iree_make_string_view(name.data(), name.size()), &function_),
                              Runtime::Errors::Iree::Call::Initialize::Failed);
                        const iree_vm_function_signature_t signature =
                            iree_vm_function_signature(&function_);
                        iree_string_view_t arguments;
                        iree_string_view_t results;
                        check(iree_vm_function_call_get_cconv_fragments(&signature, &arguments,
                                                                        &results),
                              Runtime::Errors::Iree::Call::Initialize::Failed);
                        iree_vm_list_t *inputs = nullptr;
                        check(iree_vm_list_create(iree_vm_make_undefined_type_def(), arguments.size,
                                                  iree_allocator_system(), &inputs),
                              Runtime::Errors::Iree::Call::Initialize::Failed);
                        inputs_.reset(inputs);
                        iree_vm_list_t *outputs = nullptr;
                        check(iree_vm_list_create(iree_vm_make_undefined_type_def(), results.size,
                                                  iree_allocator_system(), &outputs),
                              Runtime::Errors::Iree::Call::Initialize::Failed);
                        outputs_.reset(outputs);
                    }

                    void push(iree_hal_buffer_view_t *view) {
                        iree_vm_ref_t value{};
                        check(iree_vm_ref_wrap_assign(view, iree_hal_buffer_view_type(), &value),
                              Runtime::Errors::Iree::Call::Input::Push::Failed);
                        check(iree_vm_list_push_ref_retain(inputs_.get(), &value),
                              Runtime::Errors::Iree::Call::Input::Push::Failed);
                    }

                    void reset() noexcept {
                        iree_vm_list_clear(inputs_.get());
                        iree_vm_list_clear(outputs_.get());
                    }

                    [[nodiscard]] iree_status_t invoke() {
                        return iree_vm_invoke(session_.context(), function_,
                                              IREE_VM_INVOCATION_FLAG_NONE, nullptr, inputs_.get(),
                                              outputs_.get(), iree_allocator_system());
                    }

                    [[nodiscard]] iree_host_size_t output_count() const noexcept {
                        return iree_vm_list_size(outputs_.get());
                    }

                    Iree::View::Handle pop() {
                        iree_vm_ref_t value{};
                        check(iree_vm_list_pop_front_ref_move(outputs_.get(), &value),
                              Runtime::Errors::Iree::Call::Output::Pop::Failed);
                        iree_hal_buffer_view_t *view = nullptr;
                        check(iree_hal_buffer_view_check_deref(value, &view),
                              Runtime::Errors::Iree::Call::Output::Pop::Failed);
                        return Iree::View::Handle(view);
                    }

                private:
                    Session &session_;
                    iree_vm_function_t function_{};
                    List::Handle inputs_;
                    List::Handle outputs_;
            };
        } // namespace Iree

        uint64_t val_to_u64(const wasmtime::Val &v) {
            switch (v.kind()) {
            case wasmtime::ValKind::I32:
                return static_cast<uint64_t>(static_cast<uint32_t>(v.i32()));
            case wasmtime::ValKind::I64:
                return static_cast<uint64_t>(v.i64());
            case wasmtime::ValKind::F32: {
                float f = v.f32();
                uint32_t bits;
                std::memcpy(&bits, &f, sizeof(bits));
                return bits;
            }
            case wasmtime::ValKind::F64: {
                double d = v.f64();
                uint64_t bits;
                std::memcpy(&bits, &d, sizeof(bits));
                return bits;
            }
            default:
                raise(Runtime::Errors::Wasm::Value::Argument::Type::Unsupported);
            }
        }

        wasmtime::Val u64_to_val(uint64_t u, wasmtime::ValKind kind) {
            switch (kind) {
            case wasmtime::ValKind::I32:
                return wasmtime::Val(static_cast<int32_t>(static_cast<uint32_t>(u)));
            case wasmtime::ValKind::I64:
                return wasmtime::Val(static_cast<int64_t>(u));
            case wasmtime::ValKind::F32: {
                uint32_t bits = static_cast<uint32_t>(u);
                float f;
                std::memcpy(&f, &bits, sizeof(f));
                return wasmtime::Val(f);
            }
            case wasmtime::ValKind::F64: {
                double d;
                std::memcpy(&d, &u, sizeof(d));
                return wasmtime::Val(d);
            }
            default:
                raise(Runtime::Errors::Wasm::Value::Result::Type::Unsupported);
            }
        }

        Mln::Sdk::Native::Value::Kind native_value_kind(wasmtime::ValKind kind) {
            switch (kind) {
            case wasmtime::ValKind::I32:
                return Mln::Sdk::Native::Value::Kind::i32;
            case wasmtime::ValKind::I64:
                return Mln::Sdk::Native::Value::Kind::i64;
            case wasmtime::ValKind::F32:
                return Mln::Sdk::Native::Value::Kind::f32;
            case wasmtime::ValKind::F64:
                return Mln::Sdk::Native::Value::Kind::f64;
            default:
                raise(Runtime::Errors::Wasm::Value::Signature::Type::Unsupported);
            }
        }

        const char *native_value_kind_name(Mln::Sdk::Native::Value::Kind kind) {
            switch (kind) {
            case Mln::Sdk::Native::Value::Kind::i32:
                return "i32";
            case Mln::Sdk::Native::Value::Kind::i64:
                return "i64";
            case Mln::Sdk::Native::Value::Kind::f32:
                return "f32";
            case Mln::Sdk::Native::Value::Kind::f64:
                return "f64";
            }
            return "unknown";
        }

        void validate_native_signature(const std::string &qualified_name,
                                       const Mln::Sdk::Native::Signature *provider,
                                       const wasmtime::FuncType::Ref &wasm) {
            if (!provider)
                return;

            std::vector<Mln::Sdk::Native::Value::Kind> arguments;
            std::vector<Mln::Sdk::Native::Value::Kind> results;
            for (auto value : wasm.params())
                arguments.push_back(native_value_kind(value.kind()));
            for (auto value : wasm.results())
                results.push_back(native_value_kind(value.kind()));

            if (arguments.size() != provider->argument_count ||
                results.size() != provider->result_count)
                raise(Runtime::Errors::Native::Signature::Arity::Mismatch, qualified_name,
                      arguments.size(), results.size(), provider->argument_count,
                      provider->result_count);

            for (std::size_t i = 0; i < arguments.size(); ++i) {
                if (arguments[i] != provider->arguments[i])
                    raise(Runtime::Errors::Native::Signature::Argument::Mismatch, qualified_name, i,
                          native_value_kind_name(arguments[i]),
                          native_value_kind_name(provider->arguments[i]));
            }
            for (std::size_t i = 0; i < results.size(); ++i) {
                if (results[i] != provider->results[i])
                    raise(Runtime::Errors::Native::Signature::Result::Mismatch, qualified_name, i,
                          native_value_kind_name(results[i]),
                          native_value_kind_name(provider->results[i]));
            }
        }

    } // namespace

    Sdk::Native::Context make_native_context(Runtime &runtime) {
        return Sdk::Native::Context{
            &runtime,
            {
                +[](void *user_data, uint32_t bytes) -> uint32_t {
                    return static_cast<Runtime *>(user_data)->allocate(bytes);
                },
                +[](void *user_data, uint32_t buffer) -> void {
                    static_cast<Runtime *>(user_data)->release(buffer);
                },
                +[](void *user_data, uint32_t buffer) -> uint32_t {
                    return static_cast<Runtime *>(user_data)->length(buffer);
                },
                +[](void *user_data, uint32_t buffer, uint32_t offset, void *dst, uint32_t len)
                    -> void { static_cast<Runtime *>(user_data)->read(buffer, offset, dst, len); },
                +[](void *user_data, uint32_t buffer, uint32_t offset, const void *src,
                    uint32_t len) -> void {
                    static_cast<Runtime *>(user_data)->write(buffer, offset, src, len);
                },
                +[](void *user_data, uint32_t buffer) -> const void * {
                    return static_cast<Runtime *>(user_data)->data(buffer);
                },
            }};
    }

    namespace {

        auto make_native_trampoline(Mln::Sdk::Native::Function fn,
                                    std::vector<wasmtime::ValKind> result_kinds,
                                    Sdk::Native::Context context) {
            return [fn, result_kinds = std::move(result_kinds),
                    context](wasmtime::Caller, wasmtime::Span<const wasmtime::Val> args,
                             wasmtime::Span<wasmtime::Val> results)
                       -> wasmtime::Result<std::monostate, wasmtime::Trap> {
                std::vector<uint64_t> in(args.size());
                for (std::size_t i = 0; i < args.size(); i++)
                    in[i] = val_to_u64(args[i]);

                std::vector<uint64_t> out(result_kinds.size(), 0);
                Mln::Sdk::Native::Call call{in.data(), static_cast<uint32_t>(in.size()), out.data(),
                                            static_cast<uint32_t>(out.size())};
                auto invocation_context = context;
                fn(&invocation_context, &call);

                for (std::size_t i = 0; i < result_kinds.size(); i++)
                    results[i] = u64_to_val(out[i], result_kinds[i]);
                return std::monostate();
            };
        }

        void check(iree_status_t status, const Mln::Error &error) {
            if (iree_status_is_ok(status))
                return;

            char *message = nullptr;
            iree_host_size_t length = 0;
            iree_allocator_t allocator = iree_allocator_system();
            iree_status_to_string(status, &allocator, &message, &length);
            std::string text(message ? message : "", length);
            iree_allocator_free(allocator, message);
            iree_status_free(status);
            raise(error, text);
        }

        struct VolatileParameter {
                std::string name;
                std::span<const uint8_t> bytes;
        };

        std::vector<Weight::Descriptor> read_irpa_weight_descriptors(
            std::span<const uint8_t> parameters,
            std::vector<VolatileParameter> *volatile_parameters = nullptr) {
            iree_io_file_handle_t *file_handle_raw = nullptr;
            check(iree_io_file_handle_wrap_host_allocation(
                      IREE_IO_FILE_ACCESS_READ,
                      iree_make_byte_span(const_cast<uint8_t *>(parameters.data()),
                                          parameters.size()),
                      iree_io_file_handle_release_callback_null(), iree_allocator_system(),
                      &file_handle_raw),
                  Runtime::Errors::Iree::File::Wrap::Failed);
            Iree::File::Handle file_handle(file_handle_raw);

            iree_io_parameter_index_t *index_raw = nullptr;
            check(iree_io_parameter_index_create(iree_allocator_system(), &index_raw),
                  Runtime::Errors::Iree::Parameter::Index::Create::Failed);
            Iree::Index::Handle index(index_raw);
            check(iree_io_parse_irpa_index(file_handle.get(), index.get(), iree_allocator_system()),
                  Runtime::Errors::Iree::Parameter::Index::Parse::Failed);

            std::vector<Weight::Descriptor> descriptors;
            const iree_host_size_t count = iree_io_parameter_index_count(index.get());
            descriptors.reserve(count);
            for (iree_host_size_t i = 0; i < count; ++i) {
                const iree_io_parameter_index_entry_t *entry = nullptr;
                check(iree_io_parameter_index_get(index.get(), i, &entry),
                      Runtime::Errors::Iree::Parameter::Index::Entry::Read::Failed);
                const std::string name(entry->key.data, entry->key.size);
                if (entry->metadata.data_length == 0)
                    raise(Runtime::Errors::Irpa::Metadata::Missing, name);
                Weight::Descriptor descriptor = Weight::decode_weight_metadata(
                    std::span<const uint8_t>(entry->metadata.data, entry->metadata.data_length));
                if (descriptor.name != name)
                    raise(Runtime::Errors::Irpa::Metadata::Name::Mismatch, descriptor.name, name);
                if (descriptor.byte_length != entry->length)
                    raise(Runtime::Errors::Irpa::Metadata::Size::Mismatch, name);
                if (volatile_parameters != nullptr &&
                    descriptor.residency == Weight::Residency::Volatile) {
                    if (entry->type != IREE_IO_PARAMETER_INDEX_ENTRY_STORAGE_TYPE_FILE ||
                        entry->storage.file.offset > parameters.size() ||
                        entry->length > parameters.size() - entry->storage.file.offset)
                        raise(Runtime::Errors::Irpa::Metadata::Size::Mismatch, name);
                    volatile_parameters->push_back(VolatileParameter{
                        name,
                        parameters.subspan(static_cast<std::size_t>(entry->storage.file.offset),
                                           static_cast<std::size_t>(entry->length))});
                }
                descriptors.push_back(std::move(descriptor));
            }
            return descriptors;
        }

        void coalesce(std::vector<std::span<const uint8_t>> &ranges) {
            std::ranges::sort(ranges, {}, [](const auto bytes) {
                return reinterpret_cast<std::uintptr_t>(bytes.data());
            });
            std::vector<std::span<const uint8_t>> merged;
            merged.reserve(ranges.size());
            for (const auto bytes : ranges) {
                if (bytes.empty())
                    continue;
                const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
                const auto end = begin + bytes.size();
                if (!merged.empty()) {
                    const auto previous_begin =
                        reinterpret_cast<std::uintptr_t>(merged.back().data());
                    const auto previous_end = previous_begin + merged.back().size();
                    if (begin <= previous_end) {
                        merged.back() = std::span<const uint8_t>(
                            merged.back().data(), std::max(previous_end, end) - previous_begin);
                        continue;
                    }
                }
                merged.push_back(bytes);
            }
            ranges = std::move(merged);
        }

        void discard_volatile(const Package::File &archive,
                              const std::vector<VolatileParameter> &parameters,
                              std::string_view scope = {}) {
            std::vector<std::span<const uint8_t>> ranges;
            ranges.reserve(parameters.size());
            for (const VolatileParameter &parameter : parameters) {
                if (!scope.empty() &&
                    !(parameter.name == scope ||
                      (parameter.name.starts_with(scope) && parameter.name.size() > scope.size() &&
                       parameter.name[scope.size()] == '.')))
                    continue;
                ranges.push_back(parameter.bytes);
            }
            coalesce(ranges);
            for (const auto bytes : ranges)
                archive.discard(bytes);
        }

        std::unique_ptr<Iree::Session>
        create_base_session(iree_runtime_instance_t *instance,
                            const std::vector<std::string> &device_specs,
                            const Configuration::Runtime &configuration,
                            std::vector<Iree::Device::Handle> *retained_devices,
                            std::vector<std::string> *selected_uris,
                            std::vector<Cuda::Context> *selected_contexts) {
            if (device_specs.empty())
                raise(Runtime::Errors::Session::Device::Missing);

            const iree_allocator_t allocator = iree_runtime_instance_host_allocator(instance);
            iree_hal_driver_registry_t *registry = iree_runtime_instance_driver_registry(instance);

            iree_async_proactor_pool_t *proactor_pool_raw = nullptr;
            check(iree_async_proactor_pool_create(iree_numa_node_count(), /*node_ids=*/nullptr,
                                                  iree_async_proactor_pool_options_default(),
                                                  allocator, &proactor_pool_raw),
                  Runtime::Errors::Iree::Proactor::Pool::Create::Failed);
            Iree::Proactor::Pool::Handle proactor_pool(proactor_pool_raw);

            iree_hal_device_create_params_t create_params = iree_hal_device_create_params_default();
            create_params.proactor_pool = proactor_pool.get();

            std::vector<Iree::Device::Handle> devices;
            devices.reserve(device_specs.size());
            selected_uris->clear();
            selected_uris->reserve(device_specs.size());
            selected_contexts->clear();
            selected_contexts->reserve(device_specs.size());
            for (const std::string &spec : device_specs) {
                iree_hal_device_t *device = nullptr;
                std::size_t begin = 0;
                while (begin <= spec.size()) {
                    const std::size_t separator = spec.find(',', begin);
                    std::string canonical_uri =
                        spec.substr(begin, separator == std::string::npos ? std::string::npos
                                                                          : separator - begin);
                    if (canonical_uri.ends_with("://0"))
                        canonical_uri.resize(canonical_uri.size() - 1);
                    iree_status_t status = iree_ok_status();
                    const auto local = configuration.backends.find("local");
                    if (canonical_uri.starts_with("local-task://") &&
                        local != configuration.backends.end() && local->second.threads) {
                        if (*local->second.threads > IREE_TASK_TOPOLOGY_MAX_GROUP_COUNT)
                            raise(Runtime::Errors::Configuration::Backend::Threads::Invalid,
                                  "local");
                        iree_task_topology_t topology;
                        iree_task_topology_initialize_from_group_count(*local->second.threads,
                                                                       &topology);
                        iree_task_executor_options_t executor_options;
                        iree_task_executor_options_initialize(&executor_options);
                        iree_task_executor_t *executor = nullptr;
                        status = iree_task_executor_create(executor_options, &topology, allocator,
                                                           &executor);
                        iree_task_topology_deinitialize(&topology);

                        iree_hal_executable_plugin_manager_t *plugins = nullptr;
                        iree_hal_executable_loader_t *loaders[8]{};
                        iree_host_size_t loader_count = 0;
                        iree_hal_allocator_t *device_allocator = nullptr;
                        iree_hal_driver_t *driver = nullptr;
                        if (iree_status_is_ok(status))
                            status = iree_hal_executable_plugin_manager_create_from_flags(allocator,
                                                                                          &plugins);
                        if (iree_status_is_ok(status))
                            status = iree_hal_create_all_available_executable_loaders(
                                plugins, IREE_ARRAYSIZE(loaders), &loader_count, loaders,
                                allocator);
                        if (iree_status_is_ok(status))
                            status = iree_hal_allocator_create_heap(iree_make_cstring_view("local"),
                                                                    allocator, allocator,
                                                                    &device_allocator);
                        if (iree_status_is_ok(status)) {
                            iree_hal_task_device_params_t params;
                            iree_hal_task_device_params_initialize(&params);
                            status = iree_hal_task_driver_create(
                                iree_make_cstring_view("local-task"), &params, 1, &executor,
                                loader_count, loaders, device_allocator, allocator, &driver);
                        }
                        if (iree_status_is_ok(status))
                            status = iree_hal_driver_create_device_by_uri(
                                driver, iree_make_cstring_view(canonical_uri.c_str()),
                                &create_params, allocator, &device);
                        if (driver)
                            iree_hal_driver_release(driver);
                        if (device_allocator)
                            iree_hal_allocator_release(device_allocator);
                        for (iree_host_size_t i = 0; i < loader_count; ++i)
                            iree_hal_executable_loader_release(loaders[i]);
                        if (plugins)
                            iree_hal_executable_plugin_manager_release(plugins);
                        if (executor)
                            iree_task_executor_release(executor);
                    } else {
                        status = iree_hal_create_device(
                            registry, iree_make_cstring_view(canonical_uri.c_str()), &create_params,
                            allocator, &device);
                    }
                    if (iree_status_is_ok(status)) {
                        selected_contexts->push_back(
                            canonical_uri.starts_with("cuda://") ? Cuda::current() : nullptr);
                        selected_uris->push_back(std::move(canonical_uri));
                        break;
                    }
                    if (separator == std::string::npos)
                        check(status, Runtime::Errors::Iree::Device::Create::Failed);
                    iree_status_ignore(status);
                    begin = separator + 1;
                }
                devices.emplace_back(device);
            }

            iree_async_frontier_tracker_t *frontier_raw = nullptr;
            check(iree_async_frontier_tracker_create(iree_async_frontier_tracker_options_default(),
                                                     allocator, &frontier_raw),
                  Runtime::Errors::Iree::Session::Create::Failed);
            Iree::Frontier::Handle frontier(frontier_raw);

            iree_hal_device_group_builder_t builder;
            iree_hal_device_group_builder_initialize(&builder, frontier.get());
            Scope::Guard deinitialize_builder(
                [&builder] { iree_hal_device_group_builder_deinitialize(&builder); });
            for (const Iree::Device::Handle &device : devices) {
                check(iree_hal_device_group_builder_add_device(&builder, device.get()),
                      Runtime::Errors::Iree::Session::Create::Failed);
            }
            iree_hal_device_group_t *group_raw = nullptr;
            check(iree_hal_device_group_builder_finalize(&builder, allocator, &group_raw),
                  Runtime::Errors::Iree::Session::Create::Failed);
            deinitialize_builder.dismiss();
            Iree::Device::Group::Handle group(group_raw);

            iree_vm_context_t *context_raw = nullptr;
            check(iree_vm_context_create(iree_runtime_instance_vm_instance(instance),
                                         IREE_VM_CONTEXT_FLAG_NONE, allocator, &context_raw),
                  Runtime::Errors::Iree::Session::Create::Failed);
            Iree::Context::Handle context(context_raw);

            iree_vm_module_t *hal_module_raw = nullptr;
            check(iree_hal_module_create(iree_runtime_instance_vm_instance(instance),
                                         iree_hal_module_device_policy_default(), group.get(),
                                         IREE_HAL_MODULE_FLAG_NONE,
                                         iree_hal_module_debug_sink_stdio(stderr), allocator,
                                         &hal_module_raw),
                  Runtime::Errors::Iree::Session::Create::Failed);
            Iree::Module::Handle hal_module(hal_module_raw);
            auto session = std::make_unique<Iree::Session>(std::move(context));
            session->append(hal_module.get());

            if (retained_devices)
                *retained_devices = std::move(devices);
            return session;
        }

        std::string configured_device_uri(std::string_view uri,
                                          const Configuration::Runtime &configuration) {
            const std::size_t scheme = uri.find("://");
            const std::string_view driver = uri.substr(0, scheme);
            const std::string_view backend_name =
                driver == "local-task" || driver == "local-sync" ? "local" : driver;
            const auto configured = configuration.backends.find(backend_name);
            if (configured == configuration.backends.end())
                return std::string(uri);

            std::string device = configured->second.device;
            if (device == "local")
                device = "local-task";
            return device + "://" + std::to_string(configured->second.ordinal);
        }

        bool backend_enabled(std::string_view uri, const Configuration::Runtime &configuration) {
            if (configuration.backends.empty())
                return true;
            const std::size_t scheme = uri.find("://");
            const std::string_view driver = uri.substr(0, scheme);
            const std::string_view backend_name =
                driver == "local-task" || driver == "local-sync" ? "local" : driver;
            return configuration.backends.contains(backend_name);
        }

        std::string_view backend_name(std::string_view uri) {
            const std::size_t scheme = uri.find("://");
            const std::string_view driver = uri.substr(0, scheme);
            return driver == "local-task" || driver == "local-sync" ? "local" : driver;
        }

        std::vector<std::string> select_backend(const std::vector<std::string> &specs,
                                                std::string_view requested) {
            if (requested.empty())
                return specs;
            std::vector<std::string> selected;
            selected.reserve(specs.size());
            bool found = false;
            for (const std::string &spec : specs) {
                std::string matches;
                std::size_t begin = 0;
                while (begin <= spec.size()) {
                    const std::size_t separator = spec.find(',', begin);
                    const std::string_view alternative(
                        spec.data() + begin,
                        (separator == std::string::npos ? spec.size() : separator) - begin);
                    if (backend_name(alternative) == requested) {
                        if (!matches.empty())
                            matches.push_back(',');
                        matches.append(alternative);
                        found = true;
                    }
                    if (separator == std::string::npos)
                        break;
                    begin = separator + 1;
                }
                selected.push_back(matches.empty() ? spec : std::move(matches));
            }
            if (!found)
                raise(Runtime::Errors::Session::Device::Backend::Unavailable, requested);
            return selected;
        }

        std::vector<std::string> configure_devices(const std::vector<std::string> &specs,
                                                   const Configuration::Runtime &configuration) {
            std::vector<std::string> configured;
            configured.reserve(specs.size());
            for (const std::string &spec : specs) {
                std::string result;
                std::size_t begin = 0;
                while (begin <= spec.size()) {
                    const std::size_t separator = spec.find(',', begin);
                    const std::string_view alternative(
                        spec.data() + begin,
                        (separator == std::string::npos ? spec.size() : separator) - begin);
                    if (alternative.empty())
                        raise(Runtime::Errors::Session::Device::Missing);
                    if (backend_enabled(alternative, configuration)) {
                        if (!result.empty())
                            result.push_back(',');
                        result += configured_device_uri(alternative, configuration);
                    }
                    if (separator == std::string::npos)
                        break;
                    begin = separator + 1;
                }
                if (result.empty())
                    raise(Runtime::Errors::Session::Device::Missing);
                configured.push_back(std::move(result));
            }
            return configured;
        }

        std::vector<std::vector<std::string>> device_variants(const std::vector<std::string> &specs,
                                                              std::size_t limit) {
            std::vector<std::vector<std::string>> variants(1);
            for (const std::string &spec : specs) {
                std::vector<std::string> alternatives;
                std::size_t begin = 0;
                while (begin <= spec.size()) {
                    const std::size_t separator = spec.find(',', begin);
                    alternatives.push_back(spec.substr(begin, separator == std::string::npos
                                                                  ? std::string::npos
                                                                  : separator - begin));
                    if (separator == std::string::npos)
                        break;
                    begin = separator + 1;
                }
                std::vector<std::vector<std::string>> expanded;
                expanded.reserve(std::min(limit, variants.size() * alternatives.size()));
                for (const auto &variant : variants) {
                    for (const std::string &alternative : alternatives) {
                        auto candidate = variant;
                        candidate.push_back(alternative);
                        expanded.push_back(std::move(candidate));
                        if (expanded.size() == limit)
                            break;
                    }
                    if (expanded.size() == limit)
                        break;
                }
                variants = std::move(expanded);
            }
            return variants;
        }

        void append_parameter_index(iree_runtime_instance_t *instance, Iree::Session &session,
                                    iree_io_parameter_index_t *index) {
            iree_io_parameter_provider_t *provider_raw = nullptr;
            check(iree_io_parameter_index_provider_create(
                      iree_make_cstring_view(""), index,
                      IREE_IO_PARAMETER_INDEX_PROVIDER_DEFAULT_MAX_CONCURRENT_OPERATIONS,
                      iree_allocator_system(), &provider_raw),
                  Runtime::Errors::Iree::Parameter::Provider::Create::Failed);
            Iree::Provider::Handle provider(provider_raw);

            iree_vm_module_t *params_module_raw = nullptr;
            iree_io_parameter_provider_t *providers[] = {provider.get()};
            check(iree_io_parameters_module_create(iree_runtime_instance_vm_instance(instance), 1,
                                                   providers, iree_allocator_system(),
                                                   &params_module_raw),
                  Runtime::Errors::Iree::Parameter::Module::Create::Failed);
            Iree::Module::Handle params_module(params_module_raw);

            session.append(params_module.get());
        }

        std::unique_ptr<Iree::Session> create_memory_session(
            iree_runtime_instance_t *instance, std::span<const Package::Section> vmfbs,
            std::span<const Package::Section> parameter_archives,
            const std::vector<std::string> &device_uris,
            std::vector<Iree::Device::Handle> *retained_devices,
            std::vector<std::string> *selected_uris, std::vector<Cuda::Context> *selected_contexts,
            std::unique_ptr<Iree::Profile::Session> *retained_profile,
            const Configuration::Runtime &configuration, bool profiling) {
            std::unique_ptr<Iree::Session> session =
                create_base_session(instance, device_uris, configuration, retained_devices,
                                    selected_uris, selected_contexts);
            std::unique_ptr<Iree::Profile::Session> profile;
            if (profiling) {
                std::string output;
                bool timing = false;
                if (configuration.profile) {
                    output = configuration.profile->output;
                    timing = configuration.profile->device_timing;
                } else if (const char *legacy = std::getenv("MLN_PROFILE_OUTPUT")) {
                    output = legacy;
                    const char *legacy_timing = std::getenv("MLN_PROFILE_DEVICE_TIMING");
                    timing =
                        legacy_timing && *legacy_timing && std::string_view(legacy_timing) != "0";
                }
                profile =
                    Iree::Profile::Session::create(*retained_devices, std::move(output), timing);
            }
            try {
                if (!parameter_archives.empty()) {
                    iree_io_parameter_index_t *index_raw = nullptr;
                    check(iree_io_parameter_index_create(iree_allocator_system(), &index_raw),
                          Runtime::Errors::Iree::Parameter::Index::Create::Failed);
                    Iree::Index::Handle index(index_raw);
                    for (const Package::Section &archive : parameter_archives) {
                        iree_io_file_handle_t *file_handle_raw = nullptr;
                        check(iree_io_file_handle_wrap_host_allocation(
                                  IREE_IO_FILE_ACCESS_READ,
                                  iree_make_byte_span(const_cast<uint8_t *>(archive.bytes.data()),
                                                      archive.bytes.size()),
                                  iree_io_file_handle_release_callback_null(),
                                  iree_allocator_system(), &file_handle_raw),
                              Runtime::Errors::Iree::File::Wrap::Failed);
                        Iree::File::Handle file_handle(file_handle_raw);
                        check(iree_io_parse_irpa_index(file_handle.get(), index.get(),
                                                       iree_allocator_system()),
                              Runtime::Errors::Iree::Parameter::Index::Parse::Failed);
                    }
                    append_parameter_index(instance, *session, index.get());
                }

                for (const Package::Section &vmfb : vmfbs) {
                    iree_vm_module_t *module_raw = nullptr;
                    check(iree_vm_bytecode_module_create(
                              iree_runtime_instance_vm_instance(instance),
                              IREE_VM_BYTECODE_MODULE_FLAG_NONE,
                              iree_make_const_byte_span(vmfb.bytes.data(), vmfb.bytes.size()),
                              iree_allocator_null(), iree_runtime_instance_host_allocator(instance),
                              &module_raw),
                          Runtime::Errors::Iree::Bytecode::Memory::Append::Failed);
                    Iree::Module::Handle module(module_raw);
                    session->append(module.get());
                }
                // Device enumeration may succeed while the fully initialized
                // runtime cannot allocate. Probe after module registration so an
                // unusable variant never reaches the automatic scheduler.
                for (const Iree::Device::Handle &device : *retained_devices) {
                    const std::uint8_t byte = 0;
                    const iree_hal_dim_t shape[] = {1};
                    iree_hal_buffer_params_t params{};
                    params.type = IREE_HAL_MEMORY_TYPE_DEVICE_LOCAL;
                    params.usage = IREE_HAL_BUFFER_USAGE_DEFAULT;
                    iree_hal_buffer_view_t *view_raw = nullptr;
                    check(iree_hal_buffer_view_allocate_buffer_copy(
                              device.get(), iree_hal_device_allocator(device.get()), 1, shape,
                              IREE_HAL_ELEMENT_TYPE_UINT_8, IREE_HAL_ENCODING_TYPE_DENSE_ROW_MAJOR,
                              params, iree_make_const_byte_span(&byte, sizeof(byte)), &view_raw),
                          Runtime::Errors::Iree::Call::Input::Allocate::Failed);
                    Iree::View::Handle view(view_raw);
                }
                if (retained_profile)
                    *retained_profile = std::move(profile);
                return session;
            } catch (...) {
                if (retained_devices) {
                    retained_devices->clear();
                }
                throw;
            }
        }

        namespace Typed {
            struct Input {
                    iree_hal_element_type_t element_type;
                    uint32_t element_bytes;
                    std::vector<int64_t> shape;
            };

            struct Entrypoint {
                    std::string module;
                    std::string symbol;
                    std::vector<Input> inputs;
            };

            using Entrypoints = std::unordered_map<std::string, std::vector<Entrypoint>>;
        } // namespace Typed

        namespace Application {
            struct Entrypoint {
                    std::string module;
                    std::string export_name;
                    Artifact::Runtime runtime = Artifact::Runtime::Wasm;
            };

            using Entrypoints = std::unordered_map<std::string, Entrypoint>;
            using Hooks = std::unordered_map<std::string, std::vector<Entrypoint>>;
        } // namespace Application

        Application::Entrypoints index_application_entrypoints(std::span<const uint8_t> bytes) {
            Application::Entrypoints result;
            for (Artifact::Entrypoint &entrypoint : decode_application_entrypoints(bytes))
                result.emplace(std::move(entrypoint.name),
                               Application::Entrypoint{std::move(entrypoint.module),
                                                       std::move(entrypoint.export_name),
                                                       entrypoint.runtime});
            return result;
        }

        std::unordered_map<std::string, uint32_t> index_deployment(std::span<const uint8_t> bytes,
                                                                   std::size_t device_count) {
            std::unordered_map<std::string, uint32_t> deployment;
            for (auto &[path, device] : decode_deployment(bytes, device_count))
                deployment.emplace(std::move(path), device);
            return deployment;
        }

        Typed::Entrypoints index_signatures(std::span<const uint8_t> bytes) {
            Typed::Entrypoints result;
            for (const Artifact::Signature &signature : decode_signatures(bytes)) {
                Typed::Entrypoint entrypoint{signature.module, signature.symbol, {}};
                for (const Artifact::Input &declared : signature.inputs) {
                    Typed::Input input{};
                    if (declared.dtype == "f32") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_FLOAT_32;
                        input.element_bytes = 4;
                    } else if (declared.dtype == "f16") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_FLOAT_16;
                        input.element_bytes = 2;
                    } else if (declared.dtype == "i32") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_SINT_32;
                        input.element_bytes = 4;
                    } else if (declared.dtype == "i16") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_SINT_16;
                        input.element_bytes = 2;
                    } else if (declared.dtype == "i8") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_SINT_8;
                        input.element_bytes = 1;
                    } else if (declared.dtype == "u32") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_UINT_32;
                        input.element_bytes = 4;
                    } else if (declared.dtype == "u16") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_UINT_16;
                        input.element_bytes = 2;
                    } else if (declared.dtype == "u8") {
                        input.element_type = IREE_HAL_ELEMENT_TYPE_UINT_8;
                        input.element_bytes = 1;
                    } else {
                        raise(Runtime::Errors::Graph::Dtype::Unknown, signature.path);
                    }
                    input.shape = declared.shape;
                    entrypoint.inputs.push_back(std::move(input));
                }
                result[signature.path].push_back(std::move(entrypoint));
            }
            return result;
        }

        std::unique_ptr<Wasm::Vm> load_wasm_module(Runtime &runtime,
                                                   std::span<const uint8_t> wasm_bytes,
                                                   std::string_view instance_name,
                                                   Native::Registry &native) {
            Wasm::Vm::Config config;
            config.symbols = Wasm::core_symbols(runtime);
            auto vm = std::make_unique<Wasm::Vm>(std::move(config));

            wasmtime::Module module =
                vm->compile(std::vector<uint8_t>(wasm_bytes.begin(), wasm_bytes.end()));
            for (auto import : module.imports()) {
                std::string module_name(import.module());
                if (module_name == "myelin" || module_name.rfind("wasi", 0) == 0)
                    continue;

                std::string symbol_name(import.name());
                const Mln::Sdk::Native::Symbol *symbol = native.resolve(module_name, symbol_name);
                if (!symbol)
                    raise(Runtime::Errors::Native::Signature::Symbol::Missing, module_name,
                          symbol_name);

                auto extern_ty = wasmtime::ExternType::from_import(import);
                if (!std::holds_alternative<wasmtime::FuncType::Ref>(extern_ty))
                    raise(Runtime::Errors::Wasm::Import::Not::Function, module_name, symbol_name);

                auto func_ref = std::get<wasmtime::FuncType::Ref>(extern_ty);
                validate_native_signature(module_name + "." + symbol_name, symbol->signature,
                                          func_ref);
                std::vector<wasmtime::ValKind> result_kinds;
                for (auto value : func_ref.results())
                    result_kinds.push_back(value.kind());

                wasmtime::FuncType func_type(func_ref);
                if (auto ok = vm->linker.func_new(
                        module_name, symbol_name, std::move(func_type),
                        make_native_trampoline(symbol->function, std::move(result_kinds),
                                               make_native_context(runtime)));
                    !ok)
                    raise(Runtime::Errors::Wasm::Link::Failed, std::string(ok.err().message()));
            }
            vm->instantiate(std::string(instance_name), module);
            return vm;
        }
    } // namespace

    struct Runtime::Impl {
            // A Runtime owns one IREE instance and shared session/buffer registries.
            // Serialize access at that ownership boundary; recursive locking is
            // required because Wasm/native callbacks re-enter Runtime helpers.
            mutable std::recursive_mutex mutex;
            Configuration::Runtime configuration;

            // Driver factories registered with IREE may call into their shared
            // libraries until the IREE instance is destroyed. Declaration
            // order keeps those libraries alive for the complete instance lifetime.
            Mln::Driver::Registry drivers;
            Iree::Instance::Handle instance;
            Sdk::Native::Context native_context{};

            struct Sandbox;
            struct Stream {
                    struct Sink {
                            Runtime::Session::Output::Callback callback;
                            std::exception_ptr error;
                            bool emitted = false;
                    };

                    class Guard {
                        public:
                            Guard(Sandbox &session, Sink &sink);
                            ~Guard();
                            Guard(const Guard &) = delete;
                            Guard &operator=(const Guard &) = delete;

                        private:
                            Sandbox *session_;
                    };
            };

            struct Sandbox {
                    struct Resource {
                            enum class Kind {
                                State,
                                Asset,
                            };

                            Impl *owner = nullptr;
                            uint32_t id = 0;
                            Kind kind = Kind::State;
                            std::shared_ptr<const void> lifetime;

                            Resource(Impl &owner, uint32_t id, Kind kind = Kind::State);
                            Resource(Resource &&other) noexcept;
                            Resource &operator=(Resource &&other) noexcept;
                            Resource(const Resource &) = delete;
                            Resource &operator=(const Resource &) = delete;
                            ~Resource();

                        private:
                            void reset() noexcept;
                    };

                    struct Load {
                            struct Variant {
                                    struct Metric {
                                            uint64_t invocations = 0;
                                            double latency_ms = 0.0;
                                    };
                                    std::vector<Iree::Device::Handle> devices;
                                    std::unique_ptr<Iree::Profile::Session> profile;
                                    std::unique_ptr<Iree::Session> handle;
                                    std::unordered_map<std::string, std::unique_ptr<Iree::Call>>
                                        calls;
                                    std::vector<std::string> device_uris;
                                    std::vector<Cuda::Context> cuda_contexts;
                                    uint64_t invocations = 0;
                                    double latency_ms = 0.0;
                                    std::size_t inflight = 0;
                                    std::unordered_map<std::string, Metric> metrics;
                                    std::shared_ptr<std::mutex> execution =
                                        std::make_shared<std::mutex>();
                            };

                            std::shared_ptr<::Mln::Package::File> archive;
                            Mln::Native::Registry native;
                            std::vector<Variant> variants;
                            std::unordered_map<std::string, uint32_t> deployment;
                            Typed::Entrypoints typed_entrypoints;
                            std::unordered_map<std::string, std::unique_ptr<Wasm::Vm>> wasm_modules;
                            Application::Entrypoints entrypoints;
                            Application::Hooks hooks;
                            std::vector<VolatileParameter> volatile_parameters;
                            std::unordered_set<std::string> pending_volatile_scopes;
                    };

                    std::unordered_map<uint32_t, Load> loads;
                    std::unordered_map<std::string, uint32_t> entrypoints;
                    std::unordered_map<std::string, Resource> states;
                    std::vector<Resource> assets;

                    Stream::Sink *sink = nullptr;
                    Load *active_load = nullptr;
                    std::shared_ptr<std::atomic_bool> cancellation =
                        std::make_shared<std::atomic_bool>(false);
                    mutable std::recursive_mutex execution;
            };

            std::size_t select(Sandbox::Load &load, std::string_view path) const {
                if (load.variants.empty())
                    raise(Runtime::Errors::Session::Device::Missing);

                const auto live_bytes = [](const Sandbox::Load::Variant &variant) {
                    uint64_t live = 0;
#if IREE_STATISTICS_ENABLE
                    for (const Iree::Device::Handle &device : variant.devices) {
                        iree_hal_allocator_statistics_t statistics{};
                        iree_hal_allocator_query_statistics(iree_hal_device_allocator(device.get()),
                                                            &statistics);
                        const uint64_t host =
                            statistics.host_bytes_allocated >= statistics.host_bytes_freed
                                ? statistics.host_bytes_allocated - statistics.host_bytes_freed
                                : 0;
                        const uint64_t device_bytes =
                            statistics.device_bytes_allocated >= statistics.device_bytes_freed
                                ? statistics.device_bytes_allocated - statistics.device_bytes_freed
                                : 0;
                        live += host + device_bytes;
                    }
#else
                    (void)variant;
#endif
                    return live;
                };
                const auto available = [&](const Sandbox::Load::Variant &variant) {
                    return !configuration.memory || *configuration.memory == 0 ||
                           live_bytes(variant) < *configuration.memory;
                };

                if (configuration.scheduler == Configuration::Scheduler::Static) {
                    if (!available(load.variants.front()))
                        throw std::runtime_error("runtime.memory budget exhausted");
                    return 0;
                }
                for (std::size_t index = 0; index < load.variants.size(); ++index)
                    if (available(load.variants[index]) &&
                        !load.variants[index].metrics.contains(std::string(path)))
                        return index;

                std::size_t selected = 0;
                double selected_score = std::numeric_limits<double>::infinity();
                for (std::size_t index = 0; index < load.variants.size(); ++index) {
                    const auto &variant = load.variants[index];
                    if (!available(variant))
                        continue;
                    const auto metric = variant.metrics.find(std::string(path));
                    double score =
                        metric == variant.metrics.end() ? 0.0 : metric->second.latency_ms;
                    if (configuration.scheduler == Configuration::Scheduler::Throughput ||
                        configuration.scheduler == Configuration::Scheduler::Adaptive)
                        score *= static_cast<double>(variant.inflight + 1);
                    if (configuration.scheduler == Configuration::Scheduler::Adaptive &&
                        configuration.memory && *configuration.memory > 0) {
                        const uint64_t live = live_bytes(variant);
                        score *= 1.0 + static_cast<double>(live) /
                                           static_cast<double>(*configuration.memory);
                    }
                    if (score < selected_score) {
                        selected = index;
                        selected_score = score;
                    }
                }
                if (!std::isfinite(selected_score))
                    throw std::runtime_error("runtime.memory budget exhausted for every variant");
                return selected;
            }

            struct Storage {
                    struct Bytes {
                            std::vector<uint8_t> bytes;
                    };

                    struct Mapping {
                            std::span<const uint8_t> bytes;
                            std::shared_ptr<const void> owner;
                    };

                    struct View {
                            // Device-resident storage outlives its producing Session when a
                            // Buffer is retained by application code.
                            Iree::Device::Handle device;
                            Iree::View::Handle view;
                    };

                    explicit Storage(Bytes value, std::vector<int64_t> value_shape = {})
                        : storage(std::move(value)), shape(std::move(value_shape)) {}
                    explicit Storage(Mapping value, std::vector<int64_t> value_shape = {})
                        : storage(std::move(value)), shape(std::move(value_shape)) {}
                    explicit Storage(View value) : storage(std::move(value)) {
                        const auto &view = std::get<View>(storage).view;
                        const iree_host_size_t rank = iree_hal_buffer_view_shape_rank(view.get());
                        const iree_hal_dim_t *dimensions =
                            iree_hal_buffer_view_shape_dims(view.get());
                        shape.assign(dimensions, dimensions + rank);
                    }

                    std::variant<Bytes, Mapping, View> storage;
                    std::vector<int64_t> shape;
                    std::weak_ptr<const void> sandbox_owner;

                    [[nodiscard]] uint64_t size() const {
                        return std::visit(
                            [](const auto &value) -> uint64_t {
                                using T = std::remove_cvref_t<decltype(value)>;
                                if constexpr (std::is_same_v<T, Bytes> ||
                                              std::is_same_v<T, Mapping>)
                                    return value.bytes.size();
                                else
                                    return iree_hal_buffer_view_byte_length(value.view.get());
                            },
                            storage);
                    }
            };

            std::unordered_map<uint32_t, Storage> buffers;
            uint32_t next_buffer_id = 1;
            std::unordered_map<uint32_t, Sandbox> sessions;
            uint32_t next_session_id = 1;
            uint32_t next_load_id = 1;
            void destroy_resource(uint32_t id, Sandbox::Resource::Kind kind) noexcept;
    };

    Runtime::Impl::Sandbox::Resource::Resource(Runtime::Impl &owner_value, uint32_t identifier,
                                               Kind resource_kind)
        : owner(&owner_value), id(identifier), kind(resource_kind),
          lifetime(std::make_shared<char>()) {}

    Runtime::Impl::Sandbox::Resource::Resource(Resource &&other) noexcept
        : owner(std::exchange(other.owner, nullptr)), id(std::exchange(other.id, 0)),
          kind(other.kind), lifetime(std::move(other.lifetime)) {}

    Runtime::Impl::Sandbox::Resource &
    Runtime::Impl::Sandbox::Resource::operator=(Resource &&other) noexcept {
        if (this == &other)
            return *this;
        reset();
        owner = std::exchange(other.owner, nullptr);
        id = std::exchange(other.id, 0);
        kind = other.kind;
        lifetime = std::move(other.lifetime);
        return *this;
    }

    Runtime::Impl::Sandbox::Resource::~Resource() { reset(); }

    void Runtime::Impl::Sandbox::Resource::reset() noexcept {
        if (owner != nullptr)
            owner->destroy_resource(id, kind);
        owner = nullptr;
        id = 0;
        lifetime.reset();
    }

    void Runtime::Impl::destroy_resource(uint32_t id, Sandbox::Resource::Kind kind) noexcept {
        const auto found = buffers.find(id);
        if (found == buffers.end())
            return;
        (void)kind;
        buffers.erase(found);
    }

    Runtime::Runtime() : Runtime(std::filesystem::path{}) {}

    Runtime::Runtime(const std::filesystem::path &configuration)
        : impl_(std::make_unique<Runtime::Impl>()), lifetime_(std::make_shared<char>()) {
        if (!configuration.empty())
            impl_->configuration = Configuration::load(configuration);
        else if (const auto discovered = Configuration::discover())
            impl_->configuration = Configuration::load(*discovered);
        impl_->native_context = make_native_context(*this);
        iree_runtime_instance_options_t options;
        iree_runtime_instance_options_initialize(&options);
        iree_runtime_instance_options_use_all_available_drivers(&options);
        iree_runtime_instance_t *instance_raw = nullptr;
        check(iree_runtime_instance_create(&options, iree_allocator_system(), &instance_raw),
              Runtime::Errors::Iree::Runtime::Instance::Create::Failed);
        impl_->instance.reset(instance_raw);
        impl_->drivers =
            Mln::Driver::Registry(*iree_runtime_instance_driver_registry(impl_->instance.get()));
    }

    Runtime::Impl::Stream::Guard::Guard(Runtime::Impl::Sandbox &session, Sink &sink)
        : session_(&session) {
        if (session.sink != nullptr)
            raise(Runtime::Errors::Session::Already::Invoking);
        session.sink = &sink;
    }

    Runtime::Impl::Stream::Guard::~Guard() { session_->sink = nullptr; }

    Runtime::~Runtime() = default;

    std::vector<Runtime::Device> Runtime::devices() const {
        const std::lock_guard lock(impl_->mutex);
        const iree_allocator_t allocator =
            iree_runtime_instance_host_allocator(impl_->instance.get());
        iree_hal_driver_registry_t *registry =
            iree_runtime_instance_driver_registry(impl_->instance.get());
        iree_host_size_t driver_count = 0;
        iree_hal_driver_info_t *driver_infos = nullptr;
        check(iree_hal_driver_registry_enumerate(registry, allocator, &driver_count, &driver_infos),
              Runtime::Errors::Iree::Device::Create::Failed);
        Scope::Guard free_drivers([&] { iree_allocator_free(allocator, driver_infos); });

        std::vector<Device> result;
        for (iree_host_size_t driver_index = 0; driver_index < driver_count; ++driver_index) {
            const iree_hal_driver_info_t &driver_info = driver_infos[driver_index];
            iree_hal_driver_t *driver = nullptr;
            iree_status_t status = iree_hal_driver_registry_try_create(
                registry, driver_info.driver_name, allocator, &driver);
            if (!iree_status_is_ok(status)) {
                iree_status_ignore(status);
                continue;
            }
            Scope::Guard release_driver([&] { iree_hal_driver_release(driver); });
            iree_host_size_t device_count = 0;
            iree_hal_device_info_t *device_infos = nullptr;
            check(iree_hal_driver_query_available_devices(driver, allocator, &device_count,
                                                          &device_infos),
                  Runtime::Errors::Iree::Device::Create::Failed);
            Scope::Guard free_devices([&] { iree_allocator_free(allocator, device_infos); });
            for (iree_host_size_t device_index = 0; device_index < device_count; ++device_index) {
                const iree_hal_device_info_t &device = device_infos[device_index];
                result.push_back({
                    .driver =
                        std::string(driver_info.driver_name.data, driver_info.driver_name.size),
                    .path = std::string(device.path.data, device.path.size),
                    .name = std::string(device.name.data, device.name.size),
                });
            }
        }
        return result;
    }

    [[noreturn]] void Runtime::return_bytes_trailing(std::size_t bytes) {
        raise(Runtime::Errors::Return::Bytes::Trailing, bytes);
    }

    Runtime::Session Runtime::session() { return Session(*this); }

    Runtime::Session::Session(Runtime &runtime) : runtime_(&runtime), lifetime_(runtime.lifetime_) {
        const std::lock_guard lock(runtime.impl_->mutex);
        id_ = runtime.impl_->next_session_id++;
        auto [entry, inserted] = runtime.impl_->sessions.try_emplace(id_);
        (void)inserted;
        cancellation_ = entry->second.cancellation;
    }

    Runtime::Session::Session(std::uint32_t abi_identifier) noexcept
        : id_(abi_identifier), owning_(false) {}

    Runtime::Session::Session(Session &&other) noexcept
        : runtime_(std::exchange(other.runtime_, nullptr)), lifetime_(std::move(other.lifetime_)),
          id_(std::exchange(other.id_, 0)), owning_(std::exchange(other.owning_, false)),
          cancellation_(std::move(other.cancellation_)) {}

    Runtime::Session &Runtime::Session::operator=(Session &&other) noexcept {
        if (this == &other)
            return *this;
        reset();
        runtime_ = std::exchange(other.runtime_, nullptr);
        lifetime_ = std::move(other.lifetime_);
        id_ = std::exchange(other.id_, 0);
        owning_ = std::exchange(other.owning_, false);
        cancellation_ = std::move(other.cancellation_);
        return *this;
    }

    Runtime::Session::~Session() { reset(); }

    void Runtime::Session::reset() noexcept {
        if (owning_ && runtime_ != nullptr && id_ != 0 && !lifetime_.expired())
            runtime_->unload(id_);
        runtime_ = nullptr;
        id_ = 0;
        owning_ = false;
        lifetime_.reset();
        cancellation_.reset();
    }

    Runtime::Session Runtime::Session::borrow(Sdk::Native::Context &context,
                                              std::uint32_t identifier) {
        auto *runtime = static_cast<Runtime *>(context.user_data);
        if (!runtime)
            raise(Runtime::Errors::Session::Unknown);
        const std::lock_guard lock(runtime->impl_->mutex);
        const auto found = runtime->impl_->sessions.find(identifier);
        if (found == runtime->impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        Session session(identifier);
        session.runtime_ = runtime;
        session.lifetime_ = runtime->lifetime_;
        session.owning_ = false;
        session.cancellation_ = found->second.cancellation;
        return session;
    }

    void Runtime::Session::cancel() noexcept {
        if (cancellation_)
            cancellation_->store(true, std::memory_order_release);
    }

    void Runtime::Session::load(std::string_view package_path, std::string_view backend) {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        runtime_->load(id_, std::string(package_path), backend);
    }

    std::vector<std::string> Runtime::Session::devices() const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        return runtime_->devices(id_);
    }

    std::vector<Runtime::Route> Runtime::Session::routes() const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        return runtime_->routes(id_);
    }

    std::vector<std::string> Runtime::devices(uint32_t session_id) const {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->sessions.find(session_id);
        if (found == impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        std::vector<std::string> result;
        std::unordered_set<std::string> seen;
        for (const auto &[load_id, load] : found->second.loads) {
            (void)load_id;
            for (const auto &variant : load.variants)
                for (const std::string &uri : variant.device_uris)
                    if (seen.insert(uri).second)
                        result.push_back(uri);
        }
        return result;
    }

    std::vector<Runtime::Route> Runtime::routes(uint32_t session_id) const {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->sessions.find(session_id);
        if (found == impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        std::vector<Route> result;
        for (const auto &[load_id, load] : found->second.loads) {
            (void)load_id;
            for (const auto &variant : load.variants) {
                std::string devices;
                for (const std::string &uri : variant.device_uris) {
                    if (!devices.empty())
                        devices.push_back(',');
                    devices += uri;
                }
                result.push_back({std::move(devices), variant.invocations, variant.latency_ms});
            }
        }
        return result;
    }

    void Runtime::Session::invoke(const Buffer &input, Output::Callback callback) {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);

        runtime_->invoke(id_, input, std::move(callback));
    }

    void Runtime::Session::invoke(std::string_view entrypoint, const Buffer &input,
                                  Output::Callback callback) {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);

        runtime_->invoke(id_, std::string(entrypoint), input, std::move(callback));
    }

    Buffer Runtime::Session::invoke(std::string_view entrypoint, Buffer input) const {
        const std::array<std::reference_wrapper<const Buffer>, 1> inputs{std::cref(input)};
        std::vector<Buffer> outputs = invoke(entrypoint, inputs);
        if (outputs.size() != 1)
            raise(Runtime::Errors::Graph::Single::Output::Expected);
        return std::move(outputs.front());
    }

    std::vector<Buffer> Runtime::Session::invoke(std::string_view entrypoint,
                                                 std::span<const Buffer> inputs) const {
        std::vector<std::reference_wrapper<const Buffer>> references;
        references.reserve(inputs.size());
        for (const Buffer &input : inputs)
            references.emplace_back(std::cref(input));
        return invoke(entrypoint, references);
    }

    std::vector<Buffer>
    Runtime::Session::invoke(std::string_view entrypoint,
                             std::span<const std::reference_wrapper<const Buffer>> inputs) const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);

        std::unique_lock lock(runtime_->impl_->mutex);
        auto session = runtime_->impl_->sessions.find(id_);
        if (session == runtime_->impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        std::unique_lock session_lock(session->second.execution);
        session->second.cancellation->store(false, std::memory_order_release);
        bool selected_load = false;
        if (session->second.active_load == nullptr) {
            Impl::Sandbox::Load *selected = nullptr;
            for (auto &[load_id, load] : session->second.loads) {
                (void)load_id;
                if (!load.typed_entrypoints.contains(std::string(entrypoint)))
                    continue;
                if (selected != nullptr)
                    raise(Runtime::Errors::Entrypoint::Unknown, entrypoint);
                selected = &load;
            }
            if (selected == nullptr)
                raise(Runtime::Errors::Entrypoint::Unknown, entrypoint);
            session->second.active_load = selected;
            selected_load = true;
        }
        const Scope::Guard clear_load([&session, selected_load] {
            if (selected_load) {
                session->second.active_load = nullptr;
            }
        });

        std::vector<uint32_t> input_ids;
        input_ids.reserve(inputs.size());
        for (const auto &input : inputs)
            input_ids.push_back(runtime_->retain(input.get()));
        const Scope::Guard release_inputs([this, &input_ids] {
            for (const uint32_t input : input_ids)
                runtime_->release(input);
        });

        lock.unlock();
        std::vector<uint32_t> output_ids =
            runtime_->execute(id_, std::string(entrypoint), input_ids);
        lock.lock();
        std::size_t wrapped = 0;
        const Scope::Guard release_unwrapped([this, &output_ids, &wrapped] {
            for (; wrapped < output_ids.size(); ++wrapped)
                runtime_->release(output_ids[wrapped]);
        });
        std::vector<Buffer> outputs;
        outputs.reserve(output_ids.size());
        for (; wrapped < output_ids.size(); ++wrapped) {
            const auto storage = runtime_->impl_->buffers.find(output_ids[wrapped]);
            if (storage == runtime_->impl_->buffers.end())
                raise(Runtime::Errors::Buffer::Unknown);
            outputs.push_back(Buffer::from_native(runtime_->impl_->native_context,
                                                  output_ids[wrapped], lifetime_,
                                                  storage->second.shape));
        }
        return outputs;
    }

    Runtime::Session::State Runtime::Session::state(std::string_view name,
                                                    std::size_t bytes) const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        if (bytes > std::numeric_limits<uint32_t>::max())
            raise(Runtime::Errors::Buffer::Abi::Overflow);
        const uint32_t identifier =
            runtime_->state(id_, std::string(name), static_cast<uint32_t>(bytes));
        return State(Buffer::from_native(runtime_->impl_->native_context, identifier, lifetime_));
    }

    Runtime::Session::State Runtime::Session::state(std::string_view name, Buffer value) const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        const uint32_t identifier = runtime_->retain(value);
        Scope::Guard rollback([this, identifier] { runtime_->release(identifier); });
        runtime_->set_state(id_, std::string(name), identifier);
        rollback.dismiss();
        return State(Buffer::from_native(runtime_->impl_->native_context, identifier, lifetime_));
    }

    Buffer Runtime::Session::asset(std::string_view name) const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        const uint32_t identifier = runtime_->asset(id_, std::string(name));
        return Buffer::from_native(runtime_->impl_->native_context, identifier, lifetime_);
    }

    void Runtime::Session::emit(const Buffer &value, std::size_t offset) const {
        if (runtime_ == nullptr || lifetime_.expired())
            raise(Runtime::Errors::Session::Unknown);
        if (offset > std::numeric_limits<uint32_t>::max())
            raise(Runtime::Errors::Buffer::Abi::Overflow);
        const uint32_t identifier = runtime_->retain(value);
        const Scope::Guard release([this, identifier] { runtime_->release(identifier); });
        runtime_->emit(id_, identifier, static_cast<uint32_t>(offset));
    }

    uint32_t Runtime::retain(const Mln::Buffer &buffer) {
        const std::lock_guard lock(impl_->mutex);
        const auto owner = buffer.lifetime();
        if (!owner)
            raise(Runtime::Errors::Buffer::Null);
        if (buffer.size() > std::numeric_limits<uint32_t>::max())
            raise(Runtime::Errors::Buffer::Abi::Overflow);

        const uint32_t id = impl_->next_buffer_id++;
        if (const std::byte *bytes = buffer.host_data()) {
            impl_->buffers.emplace(
                id,
                Impl::Storage{
                    Impl::Storage::Mapping{
                        std::span(reinterpret_cast<const uint8_t *>(bytes), buffer.size()), owner},
                    std::vector<int64_t>(buffer.shape().begin(), buffer.shape().end())});
        } else {
            std::vector<uint8_t> copied_bytes(buffer.size());
            buffer.read(std::as_writable_bytes(std::span(copied_bytes)));
            impl_->buffers.emplace(id, Impl::Storage{Impl::Storage::Bytes{std::move(copied_bytes)},
                                                     std::vector<int64_t>(buffer.shape().begin(),
                                                                          buffer.shape().end())});
        }
        return id;
    }

    uint32_t Runtime::allocate(uint32_t bytes) {
        const std::lock_guard lock(impl_->mutex);
        uint32_t id = impl_->next_buffer_id++;
        impl_->buffers.emplace(id,
                               Impl::Storage{Impl::Storage::Bytes{std::vector<uint8_t>(bytes)}});
        return id;
    }

    void Runtime::release(uint32_t buffer) {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->buffers.find(buffer);
        if (found == impl_->buffers.end())
            raise(Runtime::Errors::Buffer::Unknown);
        if (!found->second.sandbox_owner.expired())
            raise(Runtime::Errors::State::Owned::By::Session);
        impl_->buffers.erase(found);
    }

    uint32_t Runtime::length(uint32_t buffer) {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->buffers.find(buffer);
        if (found == impl_->buffers.end())
            raise(Runtime::Errors::Buffer::Unknown);
        const uint64_t bytes = found->second.size();
        if (bytes > std::numeric_limits<uint32_t>::max())
            raise(Runtime::Errors::Buffer::Abi::Overflow);
        return static_cast<uint32_t>(bytes);
    }

    uint8_t Runtime::byte_at(uint32_t buffer, uint32_t index) {
        uint8_t value = 0;
        read(buffer, index, &value, 1);
        return value;
    }

    void Runtime::write_byte(uint32_t buffer, uint32_t index, uint8_t value) {
        write(buffer, index, &value, 1);
    }

    void Runtime::read(uint32_t buffer, uint32_t offset, void *dst, uint32_t len) {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->buffers.find(buffer);
        if (found == impl_->buffers.end())
            raise(Runtime::Errors::Buffer::Unknown);
        const uint64_t bytes = found->second.size();
        if (len > bytes || offset > bytes - len)
            raise(Runtime::Errors::Buffer::Read::Out::Of::Bounds);
        auto &storage = found->second.storage;
        if (auto *host = std::get_if<Impl::Storage::Bytes>(&storage)) {
            std::memcpy(dst, host->bytes.data() + offset, len);
            return;
        }
        if (auto *mapped = std::get_if<Impl::Storage::Mapping>(&storage)) {
            std::memcpy(dst, mapped->bytes.data() + offset, len);
            return;
        }
        auto &device_storage = std::get<Impl::Storage::View>(storage);
        iree_hal_buffer_t *device_buffer = iree_hal_buffer_view_buffer(device_storage.view.get());
        iree_hal_device_t *device = iree_hal_buffer_allocation_placement(device_buffer).device;
        if (!device)
            raise(Runtime::Errors::Buffer::Device::Without::Placement);
        check(iree_hal_device_transfer_d2h(device, device_buffer, offset, dst, len,
                                           IREE_HAL_TRANSFER_BUFFER_FLAG_DEFAULT,
                                           iree_infinite_timeout()),
              Runtime::Errors::Iree::Transfer::Device::To::Host::Failed);
    }

    void Runtime::write(uint32_t buffer, uint32_t offset, const void *src, uint32_t len) {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->buffers.find(buffer);
        if (found == impl_->buffers.end())
            raise(Runtime::Errors::Buffer::Unknown);
        const uint64_t bytes = found->second.size();
        if (len > bytes || offset > bytes - len)
            raise(Runtime::Errors::Buffer::Write::Out::Of::Bounds);
        auto &storage = found->second.storage;
        if (auto *host = std::get_if<Impl::Storage::Bytes>(&storage)) {
            std::memcpy(host->bytes.data() + offset, src, len);
            return;
        }
        if (std::holds_alternative<Impl::Storage::Mapping>(storage))
            raise(Runtime::Errors::Buffer::Read::Only);
        auto &device_storage = std::get<Impl::Storage::View>(storage);
        iree_hal_buffer_t *device_buffer = iree_hal_buffer_view_buffer(device_storage.view.get());
        iree_hal_device_t *device = iree_hal_buffer_allocation_placement(device_buffer).device;
        if (!device)
            raise(Runtime::Errors::Buffer::Device::Without::Placement);
        check(iree_hal_device_transfer_h2d(device, src, device_buffer, offset, len,
                                           IREE_HAL_TRANSFER_BUFFER_FLAG_DEFAULT,
                                           iree_infinite_timeout()),
              Runtime::Errors::Iree::Transfer::Host::To::Device::Failed);
    }

    const void *Runtime::data(uint32_t buffer) const {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->buffers.find(buffer);
        if (found == impl_->buffers.end())
            return nullptr;
        if (const auto *host = std::get_if<Impl::Storage::Bytes>(&found->second.storage))
            return host->bytes.data();
        if (const auto *mapped = std::get_if<Impl::Storage::Mapping>(&found->second.storage))
            return mapped->bytes.data();
        return nullptr;
    }

    uint32_t Runtime::asset(uint32_t session, const std::string &name) {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->sessions.find(session);
        if (found == impl_->sessions.end() || found->second.active_load == nullptr)
            raise(Runtime::Errors::Session::Asset::Unavailable);
        const Package::Section asset =
            found->second.active_load->archive->required(Package::Section::Type::Asset, name);
        if (asset.bytes.size() > std::numeric_limits<uint32_t>::max())
            raise(Runtime::Errors::Asset::Abi::Overflow);
        const uint32_t output = impl_->next_buffer_id++;
        impl_->buffers.emplace(output, Impl::Storage{Impl::Storage::Mapping{
                                           asset.bytes, found->second.active_load->archive}});
        auto &owner =
            found->second.assets.emplace_back(*impl_, output, Impl::Sandbox::Resource::Kind::Asset);
        impl_->buffers.at(output).sandbox_owner = owner.lifetime;
        return output;
    }

    uint32_t Runtime::state(uint32_t session, const std::string &name, uint32_t bytes) {
        const std::lock_guard lock(impl_->mutex);
        if (!impl_->sessions.contains(session))
            raise(Runtime::Errors::Session::Unknown);
        if (name.empty())
            raise(Runtime::Errors::State::Name::Empty);

        auto &states = impl_->sessions.at(session).states;
        const auto found = states.find(name);
        if (found != states.end()) {
            if (length(found->second.id) != bytes)
                raise(Runtime::Errors::State::Size::Mismatch, name);
            return found->second.id;
        }

        const uint32_t id = allocate(bytes);
        auto [state, inserted] = states.try_emplace(name, *impl_, id);
        (void)inserted;
        impl_->buffers.at(id).sandbox_owner = state->second.lifetime;
        return id;
    }

    uint32_t Runtime::set_state(uint32_t session, const std::string &name, uint32_t buffer) {
        const std::lock_guard lock(impl_->mutex);
        if (!impl_->sessions.contains(session))
            raise(Runtime::Errors::Session::Unknown);
        if (name.empty())
            raise(Runtime::Errors::State::Name::Empty);
        if (!impl_->buffers.contains(buffer))
            raise(Runtime::Errors::Buffer::Unknown);

        auto &states = impl_->sessions.at(session).states;
        const auto found = states.find(name);
        if (found != states.end() && found->second.id == buffer)
            return buffer;
        if (!impl_->buffers.at(buffer).sandbox_owner.expired())
            raise(Runtime::Errors::State::Buffer::Already::Owned);

        if (found != states.end()) {
            found->second = Impl::Sandbox::Resource(*impl_, buffer);
        } else {
            states.try_emplace(name, *impl_, buffer);
        }

        impl_->buffers.at(buffer).sandbox_owner = states.at(name).lifetime;
        return buffer;
    }

    uint32_t Runtime::execute(uint32_t session_id, const std::string &path, uint32_t input_id) {
        const std::array<uint32_t, 1> inputs{input_id};
        std::vector<uint32_t> outputs = execute(session_id, path, inputs);
        if (outputs.size() != 1) {
            for (uint32_t output : outputs)
                release(output);
            raise(Runtime::Errors::Graph::Single::Output::Expected);
        }
        return outputs.front();
    }

    std::vector<uint32_t> Runtime::execute(uint32_t session_id, const std::string &path,
                                           std::span<const uint32_t> input_ids) {
        std::unique_lock lock(impl_->mutex);
        const bool trace = [] {
            const char *value = std::getenv("MLN_TRACE_RUNTIME");
            return value != nullptr && *value != '\0' && std::string_view(value) != "0";
        }();
        const auto trace_begin = std::chrono::steady_clock::now();
        const auto session_entry = impl_->sessions.find(session_id);
        if (session_entry == impl_->sessions.end() || session_entry->second.active_load == nullptr)
            raise(Runtime::Errors::Session::Unknown);
        Impl::Sandbox &entry = session_entry->second;
        if (entry.cancellation->load(std::memory_order_acquire))
            throw std::runtime_error("session invocation cancelled");
        Impl::Sandbox::Load &load = *entry.active_load;
        const std::size_t selected_variant = impl_->select(load, path);
        auto &variant = load.variants.at(selected_variant);
        ++variant.inflight;
        const Scope::Guard leave_variant([&variant] { --variant.inflight; });
        Iree::Session &session = *variant.handle;
        const auto &devices = variant.devices;
        uint32_t device_index = 0;
        if (const auto assigned = load.deployment.find(path); assigned != load.deployment.end())
            device_index = assigned->second;
        if (device_index >= devices.size())
            raise(Runtime::Errors::Session::Device::Deployment::Unknown);
        iree_hal_device_t *target_device = devices[device_index].get();
        const Cuda::Guard cuda_context(variant.cuda_contexts.at(device_index));

        const std::vector<Typed::Input> *typed_inputs = nullptr;
        std::string physical_module = "module";
        std::string physical_path = path;
        {
            if (const auto entrypoint = load.typed_entrypoints.find(path);
                entrypoint != load.typed_entrypoints.end()) {
                const Typed::Entrypoint *selected = nullptr;
                int selected_score = -1;
                for (const Typed::Entrypoint &specialization : entrypoint->second) {
                    if (specialization.inputs.size() != input_ids.size())
                        continue;
                    bool matches = true;
                    int score = 0;
                    for (std::size_t i = 0; i < input_ids.size(); ++i) {
                        const Typed::Input &input = specialization.inputs[i];
                        const auto storage = impl_->buffers.find(input_ids[i]);
                        if (storage == impl_->buffers.end()) {
                            matches = false;
                            break;
                        }
                        const std::vector<int64_t> &actual_shape = storage->second.shape;
                        const uint64_t bytes = length(input_ids[i]);
                        if (bytes % input.element_bytes != 0) {
                            matches = false;
                            break;
                        }
                        uint64_t static_elements = 1;
                        std::size_t dynamic_dimensions = 0;
                        if (!actual_shape.empty() && actual_shape.size() != input.shape.size()) {
                            matches = false;
                            break;
                        }
                        uint64_t actual_elements = 1;
                        for (std::size_t dimension_index = 0; dimension_index < input.shape.size();
                             ++dimension_index) {
                            const int64_t dimension = input.shape[dimension_index];
                            if (dimension == -1) {
                                ++dynamic_dimensions;
                            } else {
                                static_elements *= static_cast<uint64_t>(dimension);
                                ++score;
                                if (!actual_shape.empty() &&
                                    actual_shape[dimension_index] != dimension) {
                                    matches = false;
                                    break;
                                }
                            }
                            if (!actual_shape.empty()) {
                                if (actual_shape[dimension_index] < 0) {
                                    matches = false;
                                    break;
                                }
                                actual_elements *=
                                    static_cast<uint64_t>(actual_shape[dimension_index]);
                            }
                        }
                        if (!matches)
                            break;
                        const uint64_t elements = bytes / input.element_bytes;
                        if (!actual_shape.empty() && actual_elements != elements) {
                            matches = false;
                            break;
                        }
                        if (actual_shape.empty() &&
                            ((dynamic_dimensions == 0 && elements != static_elements) ||
                             (dynamic_dimensions == 1 &&
                              (static_elements == 0 || elements % static_elements != 0)) ||
                             dynamic_dimensions > 1)) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches && score > selected_score) {
                        selected = &specialization;
                        selected_score = score;
                    }
                }
                if (!selected)
                    raise(Runtime::Errors::Graph::Specialization::Missing, path);
                typed_inputs = &selected->inputs;
                physical_module = selected->module;
                physical_path = selected->symbol;
            }
        }
        if (typed_inputs && typed_inputs->size() != input_ids.size())
            raise(Runtime::Errors::Graph::Input::Count::Mismatch, path);

        std::string full_name = physical_module + "." +
                                Mln::Sdk::Native::sanitize_symbol(physical_path) +
                                (typed_inputs ? "" : "_bytes");
        auto [cached_call, inserted] = variant.calls.try_emplace(full_name);
        if (inserted)
            cached_call->second = std::make_unique<Iree::Call>(session, full_name);
        Iree::Call &call = *cached_call->second;
        call.reset();
        iree_status_t status = iree_ok_status();

        iree_hal_buffer_params_t buffer_params{};
        buffer_params.type = IREE_HAL_MEMORY_TYPE_DEVICE_LOCAL;
        buffer_params.usage = IREE_HAL_BUFFER_USAGE_DEFAULT;

        auto input_layout = [&](std::size_t input_index, uint32_t bytes) {
            struct Layout {
                    iree_hal_element_type_t element_type;
                    std::vector<iree_hal_dim_t> shape;
            } layout;
            if (!typed_inputs) {
                layout.element_type = IREE_HAL_ELEMENT_TYPE_UINT_8;
                layout.shape = {static_cast<iree_hal_dim_t>(bytes)};
                return layout;
            }

            const Typed::Input &input = typed_inputs->at(input_index);
            const auto storage = impl_->buffers.find(input_ids[input_index]);
            if (storage == impl_->buffers.end())
                raise(Runtime::Errors::Buffer::Unknown);
            const std::vector<int64_t> &actual_shape = storage->second.shape;
            layout.element_type = input.element_type;
            if (bytes % input.element_bytes != 0)
                raise(Runtime::Errors::Graph::Byte::Count::Not::Divisible, path);
            if (!actual_shape.empty()) {
                if (actual_shape.size() != input.shape.size())
                    raise(Runtime::Errors::Graph::Dynamic::Shape::Mismatch, path);
                uint64_t actual_elements = 1;
                layout.shape.reserve(actual_shape.size());
                for (std::size_t i = 0; i < actual_shape.size(); ++i) {
                    if (actual_shape[i] < 0 ||
                        (input.shape[i] != -1 && input.shape[i] != actual_shape[i]))
                        raise(Runtime::Errors::Graph::Dynamic::Shape::Mismatch, path);
                    actual_elements *= static_cast<uint64_t>(actual_shape[i]);
                    layout.shape.push_back(static_cast<iree_hal_dim_t>(actual_shape[i]));
                }
                if (actual_elements != bytes / input.element_bytes)
                    raise(Runtime::Errors::Graph::Dynamic::Shape::Mismatch, path);
                return layout;
            }

            uint64_t static_elements = 1;
            int64_t dynamic_index = -1;
            layout.shape.reserve(input.shape.size());
            for (std::size_t i = 0; i < input.shape.size(); ++i) {
                const int64_t dimension = input.shape[i];
                if (dimension == -1) {
                    if (dynamic_index != -1)
                        raise(Runtime::Errors::Graph::Multiple::Dynamic::Dimensions, path);
                    dynamic_index = static_cast<int64_t>(i);
                    layout.shape.push_back(0);
                } else {
                    static_elements *= static_cast<uint64_t>(dimension);
                    layout.shape.push_back(static_cast<iree_hal_dim_t>(dimension));
                }
            }
            const uint64_t elements = bytes / input.element_bytes;
            if (dynamic_index >= 0) {
                if (static_elements == 0 || elements % static_elements != 0)
                    raise(Runtime::Errors::Graph::Dynamic::Shape::Mismatch, path);
                layout.shape[static_cast<std::size_t>(dynamic_index)] =
                    static_cast<iree_hal_dim_t>(elements / static_elements);
            } else if (elements != static_elements) {
                raise(Runtime::Errors::Graph::Static::Shape::Mismatch, path);
            }
            return layout;
        };

        for (std::size_t input_index = 0; input_index < input_ids.size(); ++input_index) {
            const uint32_t input_id = input_ids[input_index];
            iree_hal_buffer_view_t *input_view = nullptr;
            Iree::View::Handle owned_input_view;
            const auto input_entry = impl_->buffers.find(input_id);
            if (input_entry == impl_->buffers.end())
                raise(Runtime::Errors::Buffer::Unknown);
            auto &input_storage = input_entry->second.storage;
            if (auto *device_storage = std::get_if<Impl::Storage::View>(&input_storage)) {
                iree_hal_buffer_t *source_buffer =
                    iree_hal_buffer_view_buffer(device_storage->view.get());
                iree_hal_device_t *source_device =
                    iree_hal_buffer_allocation_placement(source_buffer).device;
                if (source_device == target_device) {
                    input_view = device_storage->view.get();
                } else {
                    const uint32_t bytes = length(input_id);
                    const auto layout = input_layout(input_index, bytes);
                    iree_hal_buffer_t *target_buffer_raw = nullptr;
                    status = iree_hal_allocator_allocate_buffer(
                        iree_hal_device_allocator(target_device), buffer_params, bytes,
                        &target_buffer_raw);
                    check(status, Runtime::Errors::Iree::Call::Input::Allocate::Failed);
                    Iree::Buffer::Handle target_buffer(target_buffer_raw);
                    status = iree_hal_device_transfer_d2d(
                        target_device, source_buffer, 0, target_buffer.get(), 0, bytes,
                        IREE_HAL_TRANSFER_BUFFER_FLAG_DEFAULT, iree_infinite_timeout());
                    if (!iree_status_is_ok(status)) {
                        // Buffers owned by unrelated HAL drivers cannot generally be
                        // submitted to each other's queues. Keep the transfer in HAL,
                        // but bridge through host memory when direct device transfer is
                        // unavailable (for example Vulkan -> local-task).
                        iree_status_ignore(status);
                        std::vector<std::uint8_t> staging(bytes);
                        status = iree_hal_device_transfer_d2h(
                            source_device, source_buffer, 0, staging.data(), bytes,
                            IREE_HAL_TRANSFER_BUFFER_FLAG_DEFAULT, iree_infinite_timeout());
                        check(status, Runtime::Errors::Iree::Call::Input::Allocate::Failed);
                        status = iree_hal_device_transfer_h2d(
                            target_device, staging.data(), target_buffer.get(), 0, bytes,
                            IREE_HAL_TRANSFER_BUFFER_FLAG_DEFAULT, iree_infinite_timeout());
                        check(status, Runtime::Errors::Iree::Call::Input::Allocate::Failed);
                    }
                    status = iree_hal_buffer_view_create(target_buffer.get(), layout.shape.size(),
                                                         layout.shape.data(), layout.element_type,
                                                         IREE_HAL_ENCODING_TYPE_DENSE_ROW_MAJOR,
                                                         iree_allocator_system(), &input_view);
                    check(status, Runtime::Errors::Iree::Call::Input::Allocate::Failed);
                    owned_input_view.reset(input_view);
                }
            } else {
                const uint32_t input_size = length(input_id);
                const auto layout = input_layout(input_index, input_size);
                if (const auto *host = std::get_if<Impl::Storage::Bytes>(&input_storage)) {
                    status = iree_hal_buffer_view_allocate_buffer_copy(
                        target_device, iree_hal_device_allocator(target_device),
                        layout.shape.size(), layout.shape.data(), layout.element_type,
                        IREE_HAL_ENCODING_TYPE_DENSE_ROW_MAJOR, buffer_params,
                        iree_make_const_byte_span(host->bytes.data(), host->bytes.size()),
                        &input_view);
                } else if (const auto *mapped =
                               std::get_if<Impl::Storage::Mapping>(&input_storage)) {
                    status = iree_hal_buffer_view_allocate_buffer_copy(
                        target_device, iree_hal_device_allocator(target_device),
                        layout.shape.size(), layout.shape.data(), layout.element_type,
                        IREE_HAL_ENCODING_TYPE_DENSE_ROW_MAJOR, buffer_params,
                        iree_make_const_byte_span(mapped->bytes.data(), mapped->bytes.size()),
                        &input_view);
                } else {
                    raise(Runtime::Errors::Buffer::Unknown);
                }
                check(status, Runtime::Errors::Iree::Call::Input::Allocate::Failed);
                owned_input_view.reset(input_view);
            }

            call.push(input_view);
        }

        const std::lock_guard variant_lock(*variant.execution);
        lock.unlock();
        status = call.invoke();
        lock.lock();
        if (!iree_status_is_ok(status)) {
            try {
                check(status, Runtime::Errors::Iree::Call::Invoke::Failed);
            } catch (const std::exception &error) {
                raise(Runtime::Errors::Graph::Invocation::Failed, path, error.what());
            }
        }

        const iree_host_size_t output_count = call.output_count();
        std::vector<Iree::View::Handle> output_views;
        output_views.reserve(output_count);
        for (iree_host_size_t i = 0; i < output_count; ++i) {
            output_views.emplace_back(call.pop());
        }

        std::vector<uint32_t> output_ids;
        output_ids.reserve(output_views.size());
        for (Iree::View::Handle &output_view : output_views) {
            const uint32_t out_id = impl_->next_buffer_id++;
            iree_hal_buffer_t *output_buffer = iree_hal_buffer_view_buffer(output_view.get());
            iree_hal_device_t *output_device =
                iree_hal_buffer_allocation_placement(output_buffer).device;
            iree_hal_device_retain(output_device);
            impl_->buffers.emplace(
                out_id, Impl::Storage{Impl::Storage::View{Iree::Device::Handle(output_device),
                                                          std::move(output_view)}});
            output_ids.push_back(out_id);
        }
        for (const std::string &scope : load.pending_volatile_scopes)
            discard_volatile(*load.archive, load.volatile_parameters, scope);
        load.pending_volatile_scopes.clear();

        const std::size_t separator = path.rfind('.');
        if (separator != std::string::npos) {
            const std::string scope = path.substr(0, separator);
            const bool uses_volatile =
                std::ranges::any_of(load.volatile_parameters, [&scope](const auto &parameter) {
                    return parameter.name == scope || (parameter.name.starts_with(scope) &&
                                                       parameter.name.size() > scope.size() &&
                                                       parameter.name[scope.size()] == '.');
                });
            if (uses_volatile)
                load.pending_volatile_scopes.insert(scope);
        }
        const double milliseconds = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - trace_begin)
                                        .count();
        variant.latency_ms =
            variant.invocations == 0 ? milliseconds : variant.latency_ms * 0.8 + milliseconds * 0.2;
        ++variant.invocations;
        auto &metric = variant.metrics[path];
        metric.latency_ms =
            metric.invocations == 0 ? milliseconds : metric.latency_ms * 0.8 + milliseconds * 0.2;
        ++metric.invocations;
        if (trace) {
            std::fprintf(stderr, "runtime graph %s device=%s inputs=%zu outputs=%zu %.2f ms\n",
                         path.c_str(), variant.device_uris[device_index].c_str(), input_ids.size(),
                         output_ids.size(), milliseconds);
            std::fflush(stderr);
        }
        return output_ids;
    }

    std::vector<Weight::Descriptor> read_artifact_parameters(std::string_view path) {
        const std::shared_ptr<Package::File> archive = Package::File::open(path);
        std::vector<Weight::Descriptor> descriptors;
        for (const Package::Section &parameters :
             archive->sections(Package::Section::Type::Parameters)) {
            std::vector<Weight::Descriptor> entries =
                read_irpa_weight_descriptors(parameters.bytes);
            descriptors.insert(descriptors.end(), std::make_move_iterator(entries.begin()),
                               std::make_move_iterator(entries.end()));
        }
        return descriptors;
    }

    uint32_t Runtime::load(uint32_t session_id, const std::string &package_path,
                           std::string_view backend) {
        const std::lock_guard lock(impl_->mutex);
        const auto session_entry = impl_->sessions.find(session_id);
        if (session_entry == impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        std::error_code ec;
        const bool is_file = std::filesystem::is_regular_file(package_path, ec);
        if (ec)
            raise(Runtime::Errors::Package::Path::Inspection::Failed, package_path, ec.message());
        if (!is_file)
            raise(Runtime::Errors::Package::Not::File, package_path);

        auto archive = Package::File::open(package_path);
        const Artifact::Requirements artifact_requirements = decode_requirements(
            archive->required(Package::Section::Type::Requirements, "runtime").bytes);
        if (artifact_requirements.runtime_abi > 1 ||
            artifact_requirements.native_abi > Sdk::Native::ABI_VERSION ||
            artifact_requirements.driver_abi > Sdk::Driver::AbiVersion)
            throw std::runtime_error("artifact requires a newer MyeLiN runtime ABI");
        if (!artifact_requirements.platform.empty() &&
            artifact_requirements.platform != current_platform())
            throw std::runtime_error("artifact native platform is incompatible: " +
                                     artifact_requirements.platform + " != " + current_platform());
        const std::vector<Package::Section> vmfbs = archive->sections(Package::Section::Type::Vmfb);
        const std::vector<Package::Section> parameter_archives =
            archive->sections(Package::Section::Type::Parameters);
        const std::vector<Package::Section> wasm_modules =
            archive->sections(Package::Section::Type::Wasm);

        Application::Entrypoints application_entrypoints;
        const auto application_sections =
            archive->sections(Package::Section::Type::ApplicationEntrypoints);
        if (application_sections.size() != 1)
            raise(Runtime::Errors::Entrypoint::Section::Missing);
        application_entrypoints = index_application_entrypoints(application_sections.front().bytes);
        // Wasm orchestration may invoke an embedded graph even when the public
        // entrypoint itself belongs to the Wasm runtime.
        const bool has_graph = !vmfbs.empty() ||
                               std::ranges::any_of(application_entrypoints, [](const auto &item) {
                                   return item.second.runtime == Artifact::Runtime::Graph;
                               });
        if (has_graph && vmfbs.empty())
            archive->required(Package::Section::Type::Vmfb);
        Application::Hooks hooks;
        const auto hook_sections = archive->sections(Package::Section::Type::Hooks);
        if (hook_sections.size() > 1)
            raise(Runtime::Errors::Metadata::Section::Duplicated);
        if (!hook_sections.empty())
            for (Artifact::Hook &hook : decode_hooks(hook_sections.front().bytes))
                hooks[std::move(hook.path)].push_back(
                    {std::move(hook.module), std::move(hook.export_name)});
        std::unordered_set<std::string> available_modules;
        for (const Package::Section &module : wasm_modules)
            available_modules.emplace(module.name);
        for (const auto &[name, entrypoint] : application_entrypoints) {
            if (entrypoint.runtime == Artifact::Runtime::Wasm &&
                !available_modules.contains(entrypoint.module))
                raise(Runtime::Errors::Entrypoint::Module::Missing, name, entrypoint.module);
        }
        for (const auto &[path, chain] : hooks)
            for (const auto &hook : chain)
                if (!available_modules.contains(hook.module))
                    raise(Runtime::Errors::Hook::Module::Missing, path, hook.module);

        std::span<const uint8_t> requirements_bytes;
        for (const Package::Section &index :
             archive->sections(Package::Section::Type::WeightIndex)) {
            if (index.name == "requirements")
                requirements_bytes = index.bytes;
        }
        const std::vector<Weight::Descriptor> requirements =
            requirements_bytes.empty() ? std::vector<Weight::Descriptor>{}
                                       : Weight::decode_weight_index(
                                             requirements_bytes, Weight::Index::Kind::Requirements);
        std::vector<Weight::Descriptor> descriptors;
        std::vector<VolatileParameter> volatile_parameters;
        std::unordered_set<std::string> descriptor_names;
        for (const Package::Section &parameters : parameter_archives) {
            auto archive_descriptors =
                read_irpa_weight_descriptors(parameters.bytes, &volatile_parameters);
            for (auto &descriptor : archive_descriptors) {
                if (!descriptor_names.insert(descriptor.name).second)
                    raise(Runtime::Errors::Package::Parameter::Duplicated, descriptor.name);
                descriptors.push_back(std::move(descriptor));
            }
        }
        Weight::validate_weights(requirements, descriptors);

        std::vector<std::string> devices;
        if (has_graph) {
            const auto device_sections = archive->sections(Package::Section::Type::Devices);
            if (device_sections.size() != 1)
                archive->required(Package::Section::Type::Devices);
            devices = configure_devices(
                select_backend(decode_devices(device_sections.front().bytes), backend),
                impl_->configuration);
        }
        std::unordered_map<std::string, uint32_t> deployment;
        const auto deployment_sections = archive->sections(Package::Section::Type::Deployment);
        if (deployment_sections.size() > 1)
            raise(Runtime::Errors::Package::Deployment::Duplicated);
        if (!deployment_sections.empty())
            deployment = index_deployment(deployment_sections.front().bytes, devices.size());
        Typed::Entrypoints typed_entrypoints;
        const auto entrypoint_sections = archive->sections(Package::Section::Type::Entrypoints);
        if (entrypoint_sections.size() > 1)
            raise(Runtime::Errors::Package::Signature::Duplicated);
        if (!entrypoint_sections.empty())
            typed_entrypoints = index_signatures(entrypoint_sections.front().bytes);
        for (auto &[name, entrypoint] : application_entrypoints) {
            (void)name;
            if (entrypoint.runtime == Artifact::Runtime::Graph && entrypoint.export_name == "*") {
                if (typed_entrypoints.size() != 1)
                    raise(Runtime::Errors::Entrypoint::Name::Required);
                entrypoint.export_name = typed_entrypoints.begin()->first;
            }
        }
        for (const Package::Section &driver : archive->sections(Package::Section::Type::Driver)) {
            impl_->drivers.load(driver.name, driver.bytes, impl_->configuration);
        }
        Mln::Native::Registry native_registry;
        for (const Package::Section &native : archive->sections(Package::Section::Type::Native)) {
            native_registry.load_memory(native.name, native.bytes);
        }
        std::unordered_map<std::string, std::unique_ptr<Wasm::Vm>> vms;
        for (const Package::Section &module : wasm_modules) {
            const auto insertion =
                vms.emplace(std::string(module.name),
                            load_wasm_module(*this, module.bytes, module.name, native_registry));
            if (!insertion.second)
                raise(Runtime::Errors::Wasm::Module::Duplicated, module.name);
        }

        // Create hardware sessions after all host plugins. Some accelerator
        // APIs bind a context to the current thread while constructing a device;
        // making this the last initialization step preserves that binding for
        // the first invocation and lets the final allocation probe reject an
        // unusable variant.
        std::vector<Impl::Sandbox::Load::Variant> runtime_variants;
        std::exception_ptr last_variant_error;
        const std::size_t variant_limit =
            impl_->configuration.scheduler == Configuration::Scheduler::Static
                ? 1
                : impl_->configuration.variants;
        if (has_graph) {
            for (const auto &candidate : device_variants(devices, variant_limit)) {
                try {
                    Impl::Sandbox::Load::Variant variant;
                    variant.handle = create_memory_session(
                        impl_->instance.get(), vmfbs, parameter_archives, candidate,
                        &variant.devices, &variant.device_uris, &variant.cuda_contexts,
                        &variant.profile, impl_->configuration,
                        impl_->configuration.profiling || impl_->configuration.profile.has_value());
                    runtime_variants.push_back(std::move(variant));
                } catch (...) {
                    last_variant_error = std::current_exception();
                }
            }
            if (runtime_variants.empty()) {
                if (last_variant_error)
                    std::rethrow_exception(last_variant_error);
                raise(Runtime::Errors::Session::Device::Missing);
            }
        }

        const uint32_t load_id = impl_->next_load_id++;
        auto &logical_session = session_entry->second;
        for (const auto &[name, entrypoint] : application_entrypoints) {
            (void)entrypoint;
            if (logical_session.entrypoints.contains(name))
                raise(Runtime::Errors::Wasm::Module::Duplicated, name);
        }

        Impl::Sandbox::Load load;
        load.native = std::move(native_registry);
        load.variants = std::move(runtime_variants);
        load.deployment = std::move(deployment);
        load.typed_entrypoints = std::move(typed_entrypoints);
        load.wasm_modules = std::move(vms);
        load.entrypoints = std::move(application_entrypoints);
        load.hooks = std::move(hooks);
        load.volatile_parameters = std::move(volatile_parameters);
        load.archive = std::move(archive);
        auto updated_entrypoints = logical_session.entrypoints;
        for (const auto &[name, entrypoint] : load.entrypoints) {
            (void)entrypoint;
            updated_entrypoints.emplace(name, load_id);
        }
        logical_session.loads.emplace(load_id, std::move(load));
        logical_session.entrypoints.swap(updated_entrypoints);
        return load_id;
    }

    void Runtime::unload(uint32_t session_id) noexcept {
        try {
            const std::lock_guard lock(impl_->mutex);
            const auto found = impl_->sessions.find(session_id);
            if (found == impl_->sessions.end())
                return;
            const std::lock_guard execution(found->second.execution);
            impl_->sessions.erase(found);
        } catch (...) {
            // Session destruction is noexcept. Failure to acquire a process-local
            // mutex during teardown cannot be surfaced through this API.
        }
    }

    void Runtime::invoke(uint32_t session_id, const Buffer &input,
                         Session::Output::Callback callback) {
        std::string entrypoint;
        {
            const std::lock_guard lock(impl_->mutex);
            const auto found = impl_->sessions.find(session_id);
            if (found == impl_->sessions.end())
                raise(Runtime::Errors::Session::Unknown);
            const auto &entrypoints = found->second.entrypoints;
            if (entrypoints.size() != 1)
                raise(Runtime::Errors::Entrypoint::Name::Required);
            entrypoint = entrypoints.begin()->first;
        }
        invoke(session_id, entrypoint, input, std::move(callback));
    }

    void Runtime::invoke(uint32_t session_id, const std::string &entrypoint, const Buffer &input,
                         Session::Output::Callback callback) {
        std::unique_lock lock(impl_->mutex);
        if (!callback)
            raise(Runtime::Errors::Stream::Callback::Empty);

        const auto session_entry = impl_->sessions.find(session_id);
        if (session_entry == impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        auto &session = session_entry->second;
        std::unique_lock session_lock(session.execution);
        session.cancellation->store(false, std::memory_order_release);
        const auto selected = session.entrypoints.find(entrypoint);
        if (selected == session.entrypoints.end())
            raise(Runtime::Errors::Entrypoint::Unknown, entrypoint);
        auto &load = session.loads.at(selected->second);
        Impl::Stream::Sink sink{std::move(callback), nullptr, false};
        {
            const Impl::Stream::Guard guard(session, sink);
            session.active_load = &load;
            const Scope::Guard clear_load([&session] { session.active_load = nullptr; });
            const uint32_t input_id = retain(input);
            bool owns_input = true;
            const Scope::Guard release_input([this, input_id, &owns_input] {
                if (owns_input)
                    release(input_id);
            });
            lock.unlock();
            const uint32_t output = invoke(session_id, entrypoint, input_id);
            owns_input = false;
            const Scope::Guard release_output([this, output] { release(output); });
            if (!sink.emitted)
                emit(session_id, output, 0);
        }
        discard_volatile(*load.archive, load.volatile_parameters);
        if (sink.error)
            std::rethrow_exception(sink.error);
    }

    uint32_t Runtime::invoke(uint32_t session_id, uint32_t input) {
        const std::lock_guard lock(impl_->mutex);
        const auto found = impl_->sessions.find(session_id);
        if (found == impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        const auto &entrypoints = found->second.entrypoints;
        if (entrypoints.size() != 1)
            raise(Runtime::Errors::Entrypoint::Name::Required);
        return invoke(session_id, entrypoints.begin()->first, input);
    }

    uint32_t Runtime::invoke(uint32_t session_id, const std::string &entrypoint, uint32_t input) {
        std::unique_lock lock(impl_->mutex);
        const auto session_entry = impl_->sessions.find(session_id);
        if (session_entry == impl_->sessions.end())
            raise(Runtime::Errors::Session::Unknown);
        auto &session = session_entry->second;
        std::unique_lock session_lock(session.execution);
        const auto selected = session.entrypoints.find(entrypoint);
        if (selected == session.entrypoints.end())
            raise(Runtime::Errors::Entrypoint::Unknown, entrypoint);
        auto &load = session.loads.at(selected->second);
        const auto found = load.entrypoints.find(entrypoint);
        lock.unlock();
        uint32_t current = input;
        bool owns_current = false;
        Scope::Guard release_current([&] {
            if (owns_current)
                release(current);
        });
        if (const auto configured = load.hooks.find(entrypoint); configured != load.hooks.end()) {
            for (const Application::Entrypoint &hook : configured->second) {
                const auto hook_module = load.wasm_modules.find(hook.module);
                if (hook_module == load.wasm_modules.end())
                    raise(Runtime::Errors::Wasm::Module::Unknown, hook.module);
                auto hook_results =
                    hook_module->second->call(hook.module, hook.export_name,
                                              {wasmtime::Val(static_cast<int32_t>(session_id)),
                                               wasmtime::Val(static_cast<int32_t>(current))});
                owns_current = false;
                const uint32_t next = static_cast<uint32_t>(hook_results[0].i32());
                current = next;
                owns_current = true;
                for (const auto &variant : load.variants)
                    for (const Iree::Device::Handle &device : variant.devices)
                        check(iree_hal_device_trim(device.get()),
                              Runtime::Errors::Iree::Device::Trim::Failed);
            }
        }

        if (found->second.runtime == Artifact::Runtime::Graph) {
            const uint32_t output = execute(session_id, found->second.export_name, current);
            return output;
        }
        if (found->second.runtime == Artifact::Runtime::Native) {
            const Sdk::Native::Symbol *symbol =
                load.native.resolve(found->second.module, found->second.export_name);
            if (!symbol)
                raise(Runtime::Errors::Native::Symbol::Invalid, "artifact", found->second.module,
                      0);
            const uint64_t arguments[]{session_id, current};
            uint64_t result = 0;
            Sdk::Native::Call call{arguments, 2, &result, 1};
            symbol->function(&impl_->native_context, &call);
            owns_current = false;
            return static_cast<uint32_t>(result);
        }
        const auto module = load.wasm_modules.find(found->second.module);
        if (module == load.wasm_modules.end())
            raise(Runtime::Errors::Wasm::Module::Unknown, found->second.module);
        auto results = module->second->call(found->second.module, found->second.export_name,
                                            {wasmtime::Val(static_cast<int32_t>(session_id)),
                                             wasmtime::Val(static_cast<int32_t>(current))});
        owns_current = false;
        return static_cast<uint32_t>(results[0].i32());
    }

    void Runtime::emit(uint32_t session, uint32_t buffer, uint32_t offset) {
        const std::lock_guard lock(impl_->mutex);
        const auto entry = impl_->sessions.find(session);
        Impl::Stream::Sink *const found =
            entry == impl_->sessions.end() ? nullptr : entry->second.sink;
        if (found == nullptr)
            return;

        Impl::Stream::Sink &sink = *found;
        if (entry->second.cancellation->load(std::memory_order_acquire))
            return;
        if (sink.error)
            return;

        try {
            const uint32_t output_length = length(buffer);
            if (offset > output_length)
                raise(Runtime::Errors::Stream::Offset::Out::Of::Bounds);

            if (!sink.emitted)
                sink.emitted = true;
            if (const auto *output_data = static_cast<const uint8_t *>(data(buffer))) {
                sink.callback(
                    std::span<const uint8_t>(output_data + offset, output_length - offset));
            } else {
                std::vector<uint8_t> bytes(output_length - offset);
                if (!bytes.empty())
                    read(buffer, offset, bytes.data(), static_cast<uint32_t>(bytes.size()));
                sink.callback(bytes);
            }
        } catch (...) {
            sink.error = std::current_exception();
        }
    }

} // namespace Mln
