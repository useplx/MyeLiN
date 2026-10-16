#include "external_driver.hpp"

#include "iree/base/api.h"
#include "iree/hal/api.h"
#include "iree/hal/drivers/local_task/task_driver.h"
#include "iree/hal/local/executable_loader.h"
#include "iree/hal/local/local_executable.h"
#include "iree/task/api.h"

#include <array>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Mln::Driver {
    namespace {
        using Module = Sdk::Driver::Module;
        using Function = Sdk::Driver::Function;
        using Binding = Sdk::Driver::Binding;

        iree_status_t module_error(iree_status_code_t code, const char *message) {
            return iree_make_status(code, "%s", message && *message ? message : "driver error");
        }

        struct Executable {
                iree_hal_local_executable_t base{};
                const Module *module = nullptr;
                void *object = nullptr;
                std::vector<Function> functions;
                std::vector<std::string> names;
                std::vector<const char *> name_pointers;
                std::vector<iree_hal_executable_dispatch_attrs_v0_t> attributes;
        };

        void executable_destroy(iree_hal_executable_t *base) {
            auto *executable = reinterpret_cast<Executable *>(base);
            executable->module->destroy(executable->object);
            iree_hal_local_executable_deinitialize(&executable->base);
            delete executable;
        }

        iree_host_size_t executable_function_count(iree_hal_executable_t *base) {
            return reinterpret_cast<Executable *>(base)->functions.size();
        }

        iree_status_t executable_function_info(iree_hal_executable_t *base,
                                               iree_hal_executable_function_t function,
                                               iree_hal_executable_function_info_t *result) {
            auto *executable = reinterpret_cast<Executable *>(base);
            if (!iree_hal_executable_function_is_index_in_range(function,
                                                                 executable->functions.size()))
                return iree_make_status(IREE_STATUS_OUT_OF_RANGE, "function is out of range");
            const std::size_t index = iree_hal_executable_function_index(function);
            const Function &metadata = executable->functions[index];
            *result = {};
            result->name = iree_make_cstring_view(executable->names[index].c_str());
            result->constant_count = metadata.constant_count;
            result->binding_count = metadata.binding_count;
            result->parameter_count = metadata.constant_count + metadata.binding_count;
            result->workgroup_size[0] = 1;
            result->workgroup_size[1] = 1;
            result->workgroup_size[2] = 1;
            return iree_ok_status();
        }

        iree_status_t executable_function_parameters(
            iree_hal_executable_t *base, iree_hal_executable_function_t function,
            iree_host_size_t capacity, iree_hal_executable_function_parameter_t *result) {
            auto *executable = reinterpret_cast<Executable *>(base);
            if (!iree_hal_executable_function_is_index_in_range(function,
                                                                 executable->functions.size()))
                return iree_make_status(IREE_STATUS_OUT_OF_RANGE, "function is out of range");
            const Function &metadata =
                executable->functions[iree_hal_executable_function_index(function)];
            const std::size_t count = metadata.constant_count + metadata.binding_count;
            if (capacity < count)
                return iree_make_status(IREE_STATUS_OUT_OF_RANGE,
                                        "function parameter capacity is too small");
            for (std::size_t index = 0; index < metadata.constant_count; ++index) {
                result[index] = {};
                result[index].type = IREE_HAL_EXECUTABLE_FUNCTION_PARAMETER_TYPE_CONSTANT;
                result[index].size = sizeof(std::uint32_t);
                result[index].offset = static_cast<std::uint16_t>(index * sizeof(std::uint32_t));
            }
            for (std::size_t index = 0; index < metadata.binding_count; ++index) {
                auto &parameter = result[metadata.constant_count + index];
                parameter = {};
                parameter.type = IREE_HAL_EXECUTABLE_FUNCTION_PARAMETER_TYPE_BINDING;
                parameter.offset = static_cast<std::uint16_t>(index);
            }
            return iree_ok_status();
        }

        iree_status_t executable_lookup(iree_hal_executable_t *base, iree_string_view_t name,
                                        iree_hal_executable_function_t *result) {
            auto *executable = reinterpret_cast<Executable *>(base);
            for (std::size_t index = 0; index < executable->names.size(); ++index) {
                if (iree_string_view_equal(name,
                                           iree_make_cstring_view(executable->names[index].c_str()))) {
                    *result = iree_hal_executable_function_from_index(
                        static_cast<std::uint32_t>(index));
                    return iree_ok_status();
                }
            }
            return iree_make_status(IREE_STATUS_NOT_FOUND, "function was not found");
        }

        iree_status_t executable_lookup_global(iree_hal_executable_t *, iree_string_view_t,
                                               iree_hal_queue_affinity_t,
                                               iree_hal_buffer_t **result) {
            *result = nullptr;
            return iree_make_status(IREE_STATUS_NOT_FOUND, "driver executable has no globals");
        }

        iree_status_t executable_issue_call(
            iree_hal_local_executable_t *base, iree_host_size_t ordinal,
            const iree_hal_executable_dispatch_state_v0_t *state,
            const iree_hal_executable_workgroup_state_v0_t *workgroup, std::uint32_t) {
            auto *executable = reinterpret_cast<Executable *>(base);
            if (workgroup->workgroup_id_x != 0 || workgroup->workgroup_id_y != 0 ||
                workgroup->workgroup_id_z != 0)
                return iree_make_status(IREE_STATUS_FAILED_PRECONDITION,
                                        "external dispatch must use one workgroup");
            std::vector<Binding> bindings(state->binding_count);
            for (std::size_t index = 0; index < bindings.size(); ++index) {
                bindings[index] = {state->binding_ptrs[index], state->binding_lengths[index]};
            }
            std::array<char, 1024> error{};
            const std::int32_t status = executable->module->invoke(
                executable->object, ordinal, state->constants, state->constant_count,
                bindings.data(), bindings.size(), error.data(), error.size());
            return status == 0 ? iree_ok_status() : module_error(IREE_STATUS_INTERNAL, error.data());
        }

        const iree_hal_local_executable_vtable_t ExecutableVtable{
            .base =
                {
                    .destroy = executable_destroy,
                    .function_count = executable_function_count,
                    .function_info = executable_function_info,
                    .function_parameters = executable_function_parameters,
                    .lookup_function_by_name = executable_lookup,
                    .lookup_global_by_name = executable_lookup_global,
                },
            .issue_call = executable_issue_call,
        };

        struct Loader {
                iree_hal_executable_loader_t base{};
                const Module *module = nullptr;
                iree_allocator_t allocator{};
        };

        void loader_destroy(iree_hal_executable_loader_t *base) {
            delete reinterpret_cast<Loader *>(base);
        }

        iree_status_t loader_infer(iree_hal_executable_loader_t *,
                                   iree_hal_executable_caching_mode_t,
                                   iree_const_byte_span_t, iree_host_size_t, char *,
                                   iree_host_size_t *) {
            return iree_make_status(IREE_STATUS_INCOMPATIBLE,
                                    "external executable format cannot be inferred");
        }

        bool loader_supports(iree_hal_executable_loader_t *base,
                             iree_hal_executable_caching_mode_t,
                             iree_string_view_t format) {
            const auto *loader = reinterpret_cast<Loader *>(base);
            return iree_string_view_equal(format, iree_make_cstring_view(loader->module->format));
        }

        iree_status_t loader_load(iree_hal_executable_loader_t *base,
                                  const iree_hal_executable_params_t *parameters,
                                  iree_host_size_t, iree_hal_executable_t **result) {
            auto *loader = reinterpret_cast<Loader *>(base);
            *result = nullptr;
            std::array<char, 1024> error{};
            void *object = loader->module->load(parameters->executable_data.data,
                                                parameters->executable_data.data_length,
                                                error.data(), error.size());
            if (!object)
                return module_error(IREE_STATUS_INVALID_ARGUMENT, error.data());

            auto *executable = new (std::nothrow) Executable;
            if (!executable) {
                loader->module->destroy(object);
                return iree_make_status(IREE_STATUS_RESOURCE_EXHAUSTED,
                                        "could not allocate external executable");
            }
            executable->module = loader->module;
            executable->object = object;
            iree_hal_local_executable_initialize(&ExecutableVtable, loader->allocator,
                                                 &executable->base);

            const std::size_t count = loader->module->function_count(object);
            executable->functions.reserve(count);
            executable->names.reserve(count);
            executable->attributes.reserve(count);
            for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
                Function function{};
                error.fill('\0');
                if (loader->module->function(object, ordinal, &function, error.data(),
                                             error.size()) != 0 || !function.name) {
                    iree_hal_executable_release(
                        reinterpret_cast<iree_hal_executable_t *>(executable));
                    return module_error(IREE_STATUS_INVALID_ARGUMENT, error.data());
                }
                if (function.constant_count > UINT8_MAX || function.binding_count > UINT8_MAX) {
                    iree_hal_executable_release(
                        reinterpret_cast<iree_hal_executable_t *>(executable));
                    return iree_make_status(IREE_STATUS_OUT_OF_RANGE,
                                            "external function exceeds HAL ABI limits");
                }
                executable->functions.push_back(function);
                executable->names.emplace_back(function.name);
                iree_hal_executable_dispatch_attrs_v0_t attributes{};
                attributes.constant_count = static_cast<std::uint8_t>(function.constant_count);
                attributes.binding_count = static_cast<std::uint8_t>(function.binding_count);
                attributes.workgroup_size_x = 1;
                attributes.workgroup_size_y = 1;
                attributes.workgroup_size_z = 1;
                attributes.parameter_count = function.constant_count + function.binding_count;
                executable->attributes.push_back(attributes);
            }
            executable->name_pointers.reserve(count);
            for (const std::string &name : executable->names)
                executable->name_pointers.push_back(name.c_str());
            executable->base.export_count = count;
            executable->base.export_names = executable->name_pointers.data();
            executable->base.dispatch_attrs = executable->attributes.data();
            *result = reinterpret_cast<iree_hal_executable_t *>(executable);
            return iree_ok_status();
        }

        const iree_hal_executable_loader_vtable_t LoaderVtable{
            .destroy = loader_destroy,
            .infer_format = loader_infer,
            .query_support = loader_supports,
            .try_load = loader_load,
        };

        Loader *create_loader(const Module &module, iree_allocator_t allocator) {
            auto *loader = new Loader;
            loader->module = &module;
            loader->allocator = allocator;
            iree_hal_executable_loader_initialize(&LoaderVtable,
                                                  iree_hal_executable_import_provider_null(),
                                                  &loader->base);
            return loader;
        }

        std::string status_text(iree_status_t status) {
            char *message = nullptr;
            iree_host_size_t length = 0;
            iree_allocator_t allocator = iree_allocator_system();
            iree_status_to_string(status, &allocator, &message, &length);
            std::string result(message ? message : "", length);
            iree_allocator_free(allocator, message);
            iree_status_free(status);
            return result;
        }
    } // namespace

        struct Factory::Impl {
            const Module *module = nullptr;
            Loader *loader = nullptr;
            iree_task_executor_t *executor = nullptr;
            iree_hal_driver_info_t information{};
            iree_hal_driver_factory_t factory{};

            static iree_status_t enumerate(void *self, iree_host_size_t *count,
                                           const iree_hal_driver_info_t **information) {
                auto &impl = *static_cast<Impl *>(self);
                *count = 1;
                *information = &impl.information;
                return iree_ok_status();
            }

            static iree_status_t create(void *self, iree_string_view_t name,
                                        iree_allocator_t host_allocator,
                                        iree_hal_driver_t **result) {
                auto &impl = *static_cast<Impl *>(self);
                if (!iree_string_view_equal(name, impl.information.driver_name))
                    return iree_make_status(IREE_STATUS_UNAVAILABLE,
                                            "driver is not provided by this factory");
                iree_hal_task_device_params_t parameters;
                iree_hal_task_device_params_initialize(&parameters);
                iree_hal_allocator_t *allocator = nullptr;
                iree_status_t status = iree_hal_allocator_create_heap(
                    name, host_allocator, host_allocator, &allocator);
                if (iree_status_is_ok(status)) {
                    iree_hal_executable_loader_t *loaders[]{&impl.loader->base};
                    iree_task_executor_t *executors[]{impl.executor};
                    status = iree_hal_task_driver_create(name, &parameters, 1, executors, 1,
                                                         loaders, allocator, host_allocator,
                                                         result);
                }
                iree_hal_allocator_release(allocator);
                return status;
            }
    };

    Factory::Factory(const Module &module, iree_hal_driver_registry_t &registry)
        : impl_(std::make_unique<Impl>()) {
        impl_->module = &module;
        iree_task_topology_t topology;
        iree_task_topology_initialize_from_group_count(1, &topology);
        iree_task_executor_options_t executor_options;
        iree_task_executor_options_initialize(&executor_options);
        const iree_status_t executor_status = iree_task_executor_create(
            executor_options, &topology, iree_allocator_system(), &impl_->executor);
        iree_task_topology_deinitialize(&topology);
        if (!iree_status_is_ok(executor_status))
            throw std::runtime_error(status_text(executor_status));
        impl_->loader = create_loader(module, iree_allocator_system());
        impl_->information.driver_name = iree_make_cstring_view(module.name);
        impl_->information.full_name = iree_make_cstring_view(module.name);
        impl_->factory.self = impl_.get();
        impl_->factory.enumerate = Impl::enumerate;
        impl_->factory.try_create = Impl::create;
        const iree_status_t status =
            iree_hal_driver_registry_register_factory(&registry, &impl_->factory);
        if (!iree_status_is_ok(status)) {
            iree_hal_executable_loader_release(&impl_->loader->base);
            impl_->loader = nullptr;
            iree_task_executor_release(impl_->executor);
            impl_->executor = nullptr;
            throw std::runtime_error(status_text(status));
        }
    }

    Factory::~Factory() {
        if (impl_ && impl_->loader)
            iree_hal_executable_loader_release(&impl_->loader->base);
        if (impl_ && impl_->executor)
            iree_task_executor_release(impl_->executor);
    }

    Factory::Factory(Factory &&) noexcept = default;
    Factory &Factory::operator=(Factory &&) noexcept = default;
} // namespace Mln::Driver
