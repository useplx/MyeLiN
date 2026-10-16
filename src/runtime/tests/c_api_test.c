#include "mln/c/runtime.h"

#include <stdint.h>
#include <string.h>

int main(void) {
    if (mln_c_api_version() != MLN_C_API_VERSION)
        return 1;
    const int64_t shape[] = {2, 3};
    const uint8_t input[] = {1, 2, 3, 4, 5, 6};
    uint8_t output[sizeof(input)] = {0};
    mln_buffer_t *buffer = NULL;
    if (mln_buffer_create(sizeof(input), shape, 2, &buffer) != MLN_STATUS_OK || !buffer)
        return 6;
    if (mln_buffer_write(buffer, 0, input, sizeof(input)) != MLN_STATUS_OK ||
        mln_buffer_read(buffer, 0, output, sizeof(output)) != MLN_STATUS_OK ||
        memcmp(input, output, sizeof(input)) != 0 || mln_buffer_rank(buffer) != 2 ||
        mln_buffer_dimension(buffer, 1) != 3)
        return 7;
    mln_buffer_destroy(buffer);

    mln_runtime_t *runtime = NULL;
    if (mln_runtime_create(&runtime) != MLN_STATUS_OK || !runtime)
        return 2;

    mln_session_t *session = NULL;
    if (mln_session_create(runtime, &session) != MLN_STATUS_OK || !session)
        return 3;
    // A session owns the runtime state it needs; destroying the lightweight
    // runtime handle first must remain valid for C and foreign-language users.
    mln_runtime_destroy(runtime);
    runtime = NULL;

    if (mln_session_load(session, "") != MLN_STATUS_INVALID_ARGUMENT)
        return 4;
    const mln_diagnostic_t *diagnostic = mln_last_diagnostic();
    if (!diagnostic || !diagnostic->message ||
        strcmp(diagnostic->message, "session and artifact_path must be valid") != 0)
        return 5;

    mln_session_destroy(session);
    return 0;
}
