#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(MLN_RUNTIME_BUILD)
#define MLN_C_API __declspec(dllexport)
#else
#define MLN_C_API __declspec(dllimport)
#endif
#else
#define MLN_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MLN_C_API_VERSION 1u

typedef struct mln_runtime_t mln_runtime_t;
typedef struct mln_session_t mln_session_t;
typedef struct mln_buffer_t mln_buffer_t;

typedef enum mln_status_t {
    MLN_STATUS_OK = 0,
    MLN_STATUS_INVALID_ARGUMENT = 1,
    MLN_STATUS_RUNTIME_ERROR = 2,
    MLN_STATUS_OUT_OF_MEMORY = 3,
    MLN_STATUS_CALLBACK_ERROR = 4,
} mln_status_t;

typedef struct mln_diagnostic_t {
        const char *domain;
        uint16_t code;
        const char *message;
        const char *location;
        uint8_t severity;
} mln_diagnostic_t;

typedef struct mln_device_t {
        const char *driver;
        const char *path;
        const char *name;
} mln_device_t;

typedef struct mln_route_t {
        const char *device;
        uint64_t invocations;
        double latency_ms;
} mln_route_t;

typedef struct mln_section_t {
        uint32_t type;
        const char *name;
        uint64_t bytes;
        uint8_t optional;
} mln_section_t;

typedef struct mln_entrypoint_t {
        const char *name;
        const char *runtime;
        const char *module;
        const char *symbol;
} mln_entrypoint_t;

typedef struct mln_input_t {
        const char *dtype;
        const int64_t *shape;
        size_t rank;
} mln_input_t;

typedef struct mln_signature_t {
        const char *path;
        const char *module;
        const char *symbol;
        const mln_input_t *inputs;
        size_t input_count;
} mln_signature_t;

typedef struct mln_deployment_t {
        const char *path;
        uint32_t device;
} mln_deployment_t;

typedef struct mln_parameter_t {
        const char *name;
        const char *codec;
        const char *storage;
        uint64_t bytes;
} mln_parameter_t;

typedef struct mln_requirements_t {
        uint32_t runtime_abi;
        uint32_t native_abi;
        uint32_t driver_abi;
        const char *platform;
} mln_requirements_t;

typedef enum mln_diagnostic_action_t {
    MLN_DIAGNOSTIC_CONTINUE = 0,
    MLN_DIAGNOSTIC_IGNORE = 1,
    MLN_DIAGNOSTIC_PROMOTE_TO_ERROR = 2,
} mln_diagnostic_action_t;

typedef enum mln_diagnostic_severity_t {
    MLN_DIAGNOSTIC_WARNING = 0,
    MLN_DIAGNOSTIC_ERROR = 1,
} mln_diagnostic_severity_t;

typedef mln_diagnostic_action_t (*mln_diagnostic_callback_t)(const mln_diagnostic_t *diagnostic,
                                                             void *user_data);

MLN_C_API uint32_t mln_c_api_version(void);
MLN_C_API const mln_diagnostic_t *mln_last_diagnostic(void);
MLN_C_API const char *mln_status_string(mln_status_t status);

MLN_C_API mln_status_t mln_runtime_create(mln_runtime_t **out_runtime);
MLN_C_API mln_status_t mln_runtime_create_with_config(const char *configuration_path,
                                                      mln_runtime_t **out_runtime);
MLN_C_API void mln_runtime_destroy(mln_runtime_t *runtime);

MLN_C_API mln_status_t mln_buffer_create(size_t size, const int64_t *shape, size_t rank,
                                         mln_buffer_t **out_buffer);
MLN_C_API mln_status_t mln_buffer_clone(const mln_buffer_t *buffer, mln_buffer_t **out_buffer);
MLN_C_API void mln_buffer_destroy(mln_buffer_t *buffer);
MLN_C_API size_t mln_buffer_size(const mln_buffer_t *buffer);
MLN_C_API size_t mln_buffer_rank(const mln_buffer_t *buffer);
MLN_C_API int64_t mln_buffer_dimension(const mln_buffer_t *buffer, size_t index);
MLN_C_API mln_status_t mln_buffer_read(const mln_buffer_t *buffer, size_t offset,
                                       uint8_t *destination, size_t size);
MLN_C_API mln_status_t mln_buffer_write(mln_buffer_t *buffer, size_t offset, const uint8_t *source,
                                        size_t size);

typedef int (*mln_device_callback_t)(const mln_device_t *device, void *user_data);
typedef int (*mln_string_callback_t)(const char *value, void *user_data);
typedef int (*mln_section_callback_t)(const mln_section_t *value, void *user_data);
typedef int (*mln_entrypoint_callback_t)(const mln_entrypoint_t *value, void *user_data);
typedef int (*mln_signature_callback_t)(const mln_signature_t *value, void *user_data);
typedef int (*mln_deployment_callback_t)(const mln_deployment_t *value, void *user_data);
typedef int (*mln_parameter_callback_t)(const mln_parameter_t *value, void *user_data);
MLN_C_API mln_status_t mln_runtime_enumerate_devices(mln_runtime_t *runtime,
                                                     mln_device_callback_t callback,
                                                     void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_backends(const char *artifact_path,
                                                       mln_string_callback_t callback,
                                                       void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_sections(const char *artifact_path,
                                                       mln_section_callback_t callback,
                                                       void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_entrypoints(const char *artifact_path,
                                                          mln_entrypoint_callback_t callback,
                                                          void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_hooks(const char *artifact_path,
                                                    mln_entrypoint_callback_t callback,
                                                    void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_signatures(const char *artifact_path,
                                                         mln_signature_callback_t callback,
                                                         void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_deployment(const char *artifact_path,
                                                         mln_deployment_callback_t callback,
                                                         void *user_data);
MLN_C_API mln_status_t mln_artifact_enumerate_parameters(const char *artifact_path,
                                                         mln_parameter_callback_t callback,
                                                         void *user_data);

MLN_C_API mln_status_t mln_artifact_requirements(const char *artifact_path,
                                                 mln_requirements_t *out_requirements);

MLN_C_API mln_status_t mln_session_create(mln_runtime_t *runtime, mln_session_t **out_session);
MLN_C_API void mln_session_destroy(mln_session_t *session);
MLN_C_API mln_status_t mln_session_load(mln_session_t *session, const char *artifact_path);
MLN_C_API mln_status_t mln_session_load_backend(mln_session_t *session, const char *artifact_path,
                                                const char *backend);
MLN_C_API mln_status_t mln_session_enumerate_devices(mln_session_t *session,
                                                     mln_string_callback_t callback,
                                                     void *user_data);
typedef int (*mln_route_callback_t)(const mln_route_t *route, void *user_data);
MLN_C_API mln_status_t mln_session_enumerate_routes(mln_session_t *session,
                                                    mln_route_callback_t callback, void *user_data);
MLN_C_API mln_status_t mln_session_set_diagnostic_callback(mln_session_t *session,
                                                           mln_diagnostic_callback_t callback,
                                                           void *user_data);

MLN_C_API void mln_session_cancel(mln_session_t *session);

typedef int (*mln_output_callback_t)(const uint8_t *data, size_t size, void *user_data);

MLN_C_API mln_status_t mln_session_invoke(mln_session_t *session, const char *entrypoint,
                                          const uint8_t *input, size_t input_size,
                                          mln_output_callback_t callback, void *user_data);
typedef int (*mln_buffer_callback_t)(const mln_buffer_t *const *buffers, size_t count,
                                     void *user_data);
MLN_C_API mln_status_t mln_session_invoke_buffers(mln_session_t *session, const char *entrypoint,
                                                  const mln_buffer_t *const *inputs,
                                                  size_t input_count,
                                                  mln_buffer_callback_t callback, void *user_data);
MLN_C_API mln_status_t mln_session_state(mln_session_t *session, const char *name, size_t size,
                                         mln_buffer_t **out_buffer);
MLN_C_API mln_status_t mln_session_set_state(mln_session_t *session, const char *name,
                                             const mln_buffer_t *buffer);
MLN_C_API mln_status_t mln_session_asset(mln_session_t *session, const char *name,
                                         mln_buffer_t **out_buffer);

#ifdef __cplusplus
}
#endif
