#include "mln/c/runtime.h"

#include "mln/error.hpp"
#include "mln/runtime/metadata.hpp"
#include "mln/runtime/runtime.hpp"

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string>
#include <string_view>

struct RuntimeState {
        Mln::Runtime value;
        std::recursive_mutex mutex;

        RuntimeState() = default;
        explicit RuntimeState(const char *configuration) : value(configuration) {}
};

struct mln_runtime_t {
        mln_runtime_t() : state(std::make_shared<RuntimeState>()) {}
        explicit mln_runtime_t(const char *configuration)
            : state(std::make_shared<RuntimeState>(configuration)) {}

        std::shared_ptr<RuntimeState> state;
};

struct mln_session_t {
        explicit mln_session_t(std::shared_ptr<RuntimeState> runtime)
            : state(std::move(runtime)), value(state->value.session()) {}

        std::shared_ptr<RuntimeState> state;
        Mln::Runtime::Session value;
        std::recursive_mutex mutex;
        mln_diagnostic_callback_t diagnostic_callback = nullptr;
        void *diagnostic_user_data = nullptr;
};

struct mln_buffer_t {
        explicit mln_buffer_t(Mln::Buffer buffer) : value(std::move(buffer)) {}
        Mln::Buffer value;
};

namespace {
    struct LastDiagnostic {
            std::string domain;
            std::string message;
            mln_diagnostic_t view{};

            void clear() {
                domain.clear();
                message.clear();
                view = {};
            }

            void set(std::string diagnostic_domain, std::uint16_t code,
                     std::string diagnostic_message) {
                domain = std::move(diagnostic_domain);
                message = std::move(diagnostic_message);
                view = {domain.c_str(), code, message.c_str(), "", MLN_DIAGNOSTIC_ERROR};
            }
    };

    thread_local LastDiagnostic last_diagnostic;

    template <typename Function> mln_status_t boundary(Function &&function) noexcept {
        last_diagnostic.clear();
        try {
            function();
            return MLN_STATUS_OK;
        } catch (const Mln::Diagnostic &diagnostic) {
            last_diagnostic.set(std::string(diagnostic.error().domain()), diagnostic.error().code(),
                                std::string(diagnostic.message()));
            return MLN_STATUS_RUNTIME_ERROR;
        } catch (const std::bad_alloc &) {
            last_diagnostic.set("myelin", 0, "memory allocation failed");
            return MLN_STATUS_OUT_OF_MEMORY;
        } catch (const std::exception &error) {
            last_diagnostic.set("myelin", 0, error.what());
            return MLN_STATUS_RUNTIME_ERROR;
        } catch (...) {
            last_diagnostic.set("myelin", 0, "unknown runtime failure");
            return MLN_STATUS_RUNTIME_ERROR;
        }
    }

    mln_status_t invalid(const char *message) noexcept {
        last_diagnostic.clear();
        last_diagnostic.set("myelin", 0, message);
        return MLN_STATUS_INVALID_ARGUMENT;
    }

    Mln::DiagnosticAction dispatch_diagnostic(const Mln::Diagnostic &diagnostic, void *user_data) {
        auto &session = *static_cast<mln_session_t *>(user_data);
        if (!session.diagnostic_callback)
            return Mln::DiagnosticAction::Continue;
        const std::string domain(diagnostic.error().domain());
        const std::string message(diagnostic.message());
        const std::string location(diagnostic.location());
        const mln_diagnostic_t view{
            domain.c_str(), diagnostic.error().code(), message.c_str(), location.c_str(),
            diagnostic.severity() == Mln::Severity::Warning ? MLN_DIAGNOSTIC_WARNING
                                                            : MLN_DIAGNOSTIC_ERROR};
        switch (session.diagnostic_callback(&view, session.diagnostic_user_data)) {
        case MLN_DIAGNOSTIC_IGNORE:
            return Mln::DiagnosticAction::Ignore;
        case MLN_DIAGNOSTIC_PROMOTE_TO_ERROR:
            return Mln::DiagnosticAction::PromoteToError;
        case MLN_DIAGNOSTIC_CONTINUE:
        default:
            return Mln::DiagnosticAction::Continue;
        }
    }
} // namespace

extern "C" uint32_t mln_c_api_version() { return MLN_C_API_VERSION; }

extern "C" mln_status_t mln_buffer_create(size_t size, const int64_t *shape, size_t rank,
                                          mln_buffer_t **out_buffer) {
    if (!out_buffer || (!shape && rank))
        return invalid("shape and output must be valid");
    *out_buffer = nullptr;
    return boundary([&] {
        std::vector<int64_t> dimensions;
        if (rank)
            dimensions.assign(shape, shape + rank);
        *out_buffer = new mln_buffer_t(Mln::Buffer(size, std::move(dimensions)));
    });
}

extern "C" mln_status_t mln_buffer_clone(const mln_buffer_t *buffer, mln_buffer_t **out_buffer) {
    if (!buffer || !out_buffer)
        return invalid("buffer and output must be valid");
    *out_buffer = nullptr;
    return boundary([&] { *out_buffer = new mln_buffer_t(buffer->value.clone()); });
}

extern "C" void mln_buffer_destroy(mln_buffer_t *buffer) { delete buffer; }
extern "C" size_t mln_buffer_size(const mln_buffer_t *buffer) {
    return buffer ? buffer->value.size() : 0;
}
extern "C" size_t mln_buffer_rank(const mln_buffer_t *buffer) {
    return buffer ? buffer->value.shape().size() : 0;
}
extern "C" int64_t mln_buffer_dimension(const mln_buffer_t *buffer, size_t index) {
    return buffer && index < buffer->value.shape().size() ? buffer->value.shape()[index] : -1;
}
extern "C" mln_status_t mln_buffer_read(const mln_buffer_t *buffer, size_t offset,
                                        uint8_t *destination, size_t size) {
    if (!buffer || (!destination && size))
        return invalid("buffer and destination must be valid");
    return boundary(
        [&] { buffer->value.read(offset, std::as_writable_bytes(std::span(destination, size))); });
}
extern "C" mln_status_t mln_buffer_write(mln_buffer_t *buffer, size_t offset, const uint8_t *source,
                                         size_t size) {
    if (!buffer || (!source && size))
        return invalid("buffer and source must be valid");
    return boundary([&] { buffer->value.write(offset, std::as_bytes(std::span(source, size))); });
}

extern "C" const mln_diagnostic_t *mln_last_diagnostic() {
    return last_diagnostic.message.empty() ? nullptr : &last_diagnostic.view;
}

extern "C" const char *mln_status_string(mln_status_t status) {
    switch (status) {
    case MLN_STATUS_OK:
        return "ok";
    case MLN_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case MLN_STATUS_RUNTIME_ERROR:
        return "runtime error";
    case MLN_STATUS_OUT_OF_MEMORY:
        return "out of memory";
    case MLN_STATUS_CALLBACK_ERROR:
        return "output callback failed";
    }
    return "unknown status";
}

extern "C" mln_status_t mln_runtime_create(mln_runtime_t **out_runtime) {
    if (!out_runtime)
        return invalid("out_runtime must not be null");
    *out_runtime = nullptr;
    return boundary([&] { *out_runtime = new mln_runtime_t; });
}

extern "C" mln_status_t mln_runtime_create_with_config(const char *configuration_path,
                                                       mln_runtime_t **out_runtime) {
    if (!configuration_path || !*configuration_path || !out_runtime)
        return invalid("configuration_path and out_runtime must be valid");
    *out_runtime = nullptr;
    return boundary([&] { *out_runtime = new mln_runtime_t(configuration_path); });
}

extern "C" void mln_runtime_destroy(mln_runtime_t *runtime) {
    last_diagnostic.clear();
    delete runtime;
}

extern "C" mln_status_t mln_runtime_enumerate_devices(mln_runtime_t *runtime,
                                                      mln_device_callback_t callback,
                                                      void *user_data) {
    if (!runtime || !callback)
        return invalid("runtime and callback must not be null");
    return boundary([&] {
        const std::scoped_lock lock(runtime->state->mutex);
        for (const Mln::Runtime::Device &device : runtime->state->value.devices()) {
            const mln_device_t view{device.driver.c_str(), device.path.c_str(),
                                    device.name.c_str()};
            if (callback(&view, user_data) != 0)
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_enumerate_backends(const char *artifact_path,
                                                        mln_string_callback_t callback,
                                                        void *user_data) {
    if (!artifact_path || !*artifact_path || !callback)
        return invalid("artifact_path and callback must be valid");
    return boundary([&] {
        const Mln::Artifact::Metadata metadata = Mln::read_artifact_metadata(artifact_path);
        for (const std::string &backend : metadata.backends)
            if (callback(backend.c_str(), user_data) != 0)
                break;
    });
}

extern "C" mln_status_t mln_artifact_enumerate_sections(const char *path,
                                                        mln_section_callback_t callback,
                                                        void *user_data) {
    if (!path || !callback)
        return invalid("artifact path and callback must be valid");
    return boundary([&] {
        for (const auto &section : Mln::read_artifact_metadata(path).sections) {
            const mln_section_t value{section.raw_type, section.name.c_str(), section.bytes,
                                      static_cast<uint8_t>(section.optional)};
            if (callback(&value, user_data))
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_enumerate_entrypoints(const char *path,
                                                           mln_entrypoint_callback_t callback,
                                                           void *user_data) {
    if (!path || !callback)
        return invalid("artifact path and callback must be valid");
    return boundary([&] {
        for (const auto &entry : Mln::read_artifact_metadata(path).entrypoints) {
            const char *runtime = entry.runtime == Mln::Artifact::Runtime::Graph    ? "graph"
                                  : entry.runtime == Mln::Artifact::Runtime::Native ? "native"
                                                                                    : "wasm";
            const mln_entrypoint_t value{entry.name.c_str(), runtime, entry.module.c_str(),
                                         entry.export_name.c_str()};
            if (callback(&value, user_data))
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_enumerate_hooks(const char *path,
                                                     mln_entrypoint_callback_t callback,
                                                     void *user_data) {
    if (!path || !callback)
        return invalid("artifact path and callback must be valid");
    return boundary([&] {
        for (const auto &hook : Mln::read_artifact_metadata(path).hooks) {
            const mln_entrypoint_t value{hook.path.c_str(), "wasm", hook.module.c_str(),
                                         hook.export_name.c_str()};
            if (callback(&value, user_data))
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_enumerate_signatures(const char *path,
                                                          mln_signature_callback_t callback,
                                                          void *user_data) {
    if (!path || !callback)
        return invalid("artifact path and callback must be valid");
    return boundary([&] {
        for (const auto &signature : Mln::read_artifact_metadata(path).signatures) {
            std::vector<mln_input_t> inputs;
            inputs.reserve(signature.inputs.size());
            for (const auto &input : signature.inputs)
                inputs.push_back({input.dtype.c_str(), input.shape.data(), input.shape.size()});
            const mln_signature_t value{signature.path.c_str(), signature.module.c_str(),
                                        signature.symbol.c_str(), inputs.data(), inputs.size()};
            if (callback(&value, user_data))
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_enumerate_deployment(const char *path,
                                                          mln_deployment_callback_t callback,
                                                          void *user_data) {
    if (!path || !callback)
        return invalid("artifact path and callback must be valid");
    return boundary([&] {
        for (const auto &[route, device] : Mln::read_artifact_metadata(path).deployment) {
            const mln_deployment_t value{route.c_str(), device};
            if (callback(&value, user_data))
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_enumerate_parameters(const char *path,
                                                          mln_parameter_callback_t callback,
                                                          void *user_data) {
    if (!path || !callback)
        return invalid("artifact path and callback must be valid");
    return boundary([&] {
        for (const auto &parameter : Mln::read_artifact_metadata(path).parameters) {
            const mln_parameter_t value{parameter.name.c_str(), parameter.quant.c_str(),
                                        parameter.storage.c_str(), parameter.byte_length};
            if (callback(&value, user_data))
                break;
        }
    });
}

extern "C" mln_status_t mln_artifact_requirements(const char *path,
                                                  mln_requirements_t *out_requirements) {
    if (!path || !*path || !out_requirements)
        return invalid("artifact path and out_requirements must be valid");
    return boundary([&] {
        static thread_local std::string platform;
        const auto requirements = Mln::read_artifact_metadata(path).requirements;
        platform = requirements.platform;
        *out_requirements = {requirements.runtime_abi, requirements.native_abi,
                             requirements.driver_abi, platform.c_str()};
    });
}

extern "C" mln_status_t mln_session_create(mln_runtime_t *runtime, mln_session_t **out_session) {
    if (!runtime || !out_session)
        return invalid("runtime and out_session must not be null");
    *out_session = nullptr;
    return boundary([&] {
        const std::scoped_lock lock(runtime->state->mutex);
        *out_session = new mln_session_t(runtime->state);
    });
}

extern "C" void mln_session_destroy(mln_session_t *session) {
    last_diagnostic.clear();
    if (!session)
        return;
    // Wait for an active call, then release the lock before destroying its mutex.
    // As with other C handles, callers must prevent new calls during destruction.
    {
        const std::scoped_lock lock(session->mutex);
    }
    delete session;
}

extern "C" mln_status_t mln_session_load(mln_session_t *session, const char *artifact_path) {
    return mln_session_load_backend(session, artifact_path, nullptr);
}

extern "C" mln_status_t mln_session_load_backend(mln_session_t *session, const char *artifact_path,
                                                 const char *backend) {
    if (!session || !artifact_path || !*artifact_path)
        return invalid("session and artifact_path must be valid");
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        const Mln::ScopedDiagnosticHandler diagnostics(dispatch_diagnostic, session);
        session->value.load(artifact_path, backend ? backend : "");
    });
}

extern "C" mln_status_t mln_session_enumerate_devices(mln_session_t *session,
                                                      mln_string_callback_t callback,
                                                      void *user_data) {
    if (!session || !callback)
        return invalid("session and callback must not be null");
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        for (const std::string &device : session->value.devices())
            if (callback(device.c_str(), user_data) != 0)
                break;
    });
}

extern "C" mln_status_t mln_session_enumerate_routes(mln_session_t *session,
                                                     mln_route_callback_t callback,
                                                     void *user_data) {
    if (!session || !callback)
        return invalid("session and callback must not be null");
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        for (const Mln::Runtime::Route &route : session->value.routes()) {
            const mln_route_t value{route.device.c_str(), route.invocations, route.latency_ms};
            if (callback(&value, user_data) != 0)
                break;
        }
    });
}

extern "C" mln_status_t mln_session_set_diagnostic_callback(mln_session_t *session,
                                                            mln_diagnostic_callback_t callback,
                                                            void *user_data) {
    if (!session)
        return invalid("session must not be null");
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        session->diagnostic_callback = callback;
        session->diagnostic_user_data = user_data;
    });
}

extern "C" void mln_session_cancel(mln_session_t *session) {
    if (session)
        session->value.cancel();
}

extern "C" mln_status_t mln_session_invoke(mln_session_t *session, const char *entrypoint,
                                           const uint8_t *input, size_t input_size,
                                           mln_output_callback_t callback, void *user_data) {
    if (!session || (!input && input_size) || !callback)
        return invalid("session, input range, and callback must be valid");

    std::atomic_bool callback_failed = false;
    const mln_status_t status = boundary([&] {
        Mln::Buffer argument(input_size);
        if (input_size) {
            argument.write(std::as_bytes(std::span(input, input_size)));
        }
        const auto output = [&](std::span<const std::uint8_t> bytes) {
            if (!callback_failed.load(std::memory_order_relaxed) &&
                callback(bytes.data(), bytes.size(), user_data) != 0) {
                callback_failed.store(true, std::memory_order_relaxed);
                session->value.cancel();
            }
        };
        const std::scoped_lock lock(session->mutex);
        const Mln::ScopedDiagnosticHandler diagnostics(dispatch_diagnostic, session);
        if (entrypoint && *entrypoint)
            session->value.invoke(std::string_view(entrypoint), argument, output);
        else
            session->value.invoke(argument, output);
    });
    if (callback_failed.load(std::memory_order_relaxed)) {
        last_diagnostic.set("myelin", 0, "output callback requested cancellation");
        return MLN_STATUS_CALLBACK_ERROR;
    }
    if (status != MLN_STATUS_OK)
        return status;
    return MLN_STATUS_OK;
}

extern "C" mln_status_t mln_session_invoke_buffers(mln_session_t *session, const char *entrypoint,
                                                   const mln_buffer_t *const *inputs,
                                                   size_t input_count,
                                                   mln_buffer_callback_t callback,
                                                   void *user_data) {
    if (!session || !entrypoint || !*entrypoint || (!inputs && input_count) || !callback)
        return invalid("session, entrypoint, inputs, and callback must be valid");
    return boundary([&] {
        std::vector<std::reference_wrapper<const Mln::Buffer>> references;
        references.reserve(input_count);
        for (size_t index = 0; index < input_count; ++index) {
            if (!inputs[index])
                throw std::invalid_argument("input buffer must not be null");
            references.emplace_back(std::cref(inputs[index]->value));
        }
        const std::scoped_lock lock(session->mutex);
        auto outputs = session->value.invoke(entrypoint, references);
        std::vector<mln_buffer_t> wrappers;
        wrappers.reserve(outputs.size());
        for (auto &output : outputs)
            wrappers.emplace_back(std::move(output));
        std::vector<const mln_buffer_t *> pointers;
        pointers.reserve(wrappers.size());
        for (const auto &wrapper : wrappers)
            pointers.push_back(&wrapper);
        if (callback(pointers.data(), pointers.size(), user_data) != 0)
            throw std::runtime_error("buffer callback failed");
    });
}

extern "C" mln_status_t mln_session_state(mln_session_t *session, const char *name, size_t size,
                                          mln_buffer_t **out_buffer) {
    if (!session || !name || !*name || !out_buffer)
        return invalid("state arguments are invalid");
    *out_buffer = nullptr;
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        auto state = session->value.state(name, size);
        *out_buffer = new mln_buffer_t(state.data().clone());
    });
}

extern "C" mln_status_t mln_session_set_state(mln_session_t *session, const char *name,
                                              const mln_buffer_t *buffer) {
    if (!session || !name || !*name || !buffer)
        return invalid("state arguments are invalid");
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        (void)session->value.state(name, buffer->value.clone());
    });
}

extern "C" mln_status_t mln_session_asset(mln_session_t *session, const char *name,
                                          mln_buffer_t **out_buffer) {
    if (!session || !name || !*name || !out_buffer)
        return invalid("asset arguments are invalid");
    *out_buffer = nullptr;
    return boundary([&] {
        const std::scoped_lock lock(session->mutex);
        *out_buffer = new mln_buffer_t(session->value.asset(name));
    });
}
