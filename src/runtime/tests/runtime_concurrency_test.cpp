#include "mln/c/runtime.h"
#include "mln/runtime/runtime.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
    constexpr int thread_count = 8;
    constexpr int iterations = 100;
}

int main() {
    Mln::Runtime runtime;
    std::atomic_int failures = 0;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (int thread = 0; thread < thread_count; ++thread) {
        workers.emplace_back([&runtime, &failures, thread] {
            try {
                for (int iteration = 0; iteration < iterations; ++iteration) {
                    auto session = runtime.session();
                    auto state = session.state("counter", sizeof(std::uint32_t));
                    const std::array input{std::byte(static_cast<std::uint8_t>(thread))};
                    state.write(0, input);
                    std::array<std::byte, 1> output{};
                    state.read(0, output);
                    if (output != input)
                        ++failures;
                }
            } catch (...) {
                ++failures;
            }
        });
    }
    for (auto &worker : workers)
        worker.join();
    if (failures.load() != 0)
        return 1;

    mln_runtime_t *c_runtime = nullptr;
    if (mln_runtime_create(&c_runtime) != MLN_STATUS_OK)
        return 2;
    workers.clear();
    for (int thread = 0; thread < thread_count; ++thread) {
        workers.emplace_back([c_runtime, &failures] {
            for (int iteration = 0; iteration < iterations; ++iteration) {
                mln_session_t *session = nullptr;
                if (mln_session_create(c_runtime, &session) != MLN_STATUS_OK || !session) {
                    ++failures;
                    continue;
                }
                mln_session_destroy(session);
            }
        });
    }
    for (auto &worker : workers)
        worker.join();
    mln_runtime_destroy(c_runtime);
    return failures.load() == 0 ? 0 : 3;
}
