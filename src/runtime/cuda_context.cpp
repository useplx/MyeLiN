#include "cuda_context.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdexcept>

namespace Mln::Cuda {
    namespace {
        using Result = int;
        using Get = Result (*)(Context *);
        using Set = Result (*)(Context);

        struct Api {
                void *library = nullptr;
                Get get = nullptr;
                Set set = nullptr;

                Api() {
#if defined(_WIN32)
                    library = LoadLibraryA("nvcuda.dll");
#else
                    library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
                    if (!library)
                        throw std::runtime_error("CUDA driver library is unavailable");
#if defined(_WIN32)
                    get = reinterpret_cast<Get>(
                        GetProcAddress(static_cast<HMODULE>(library), "cuCtxGetCurrent"));
                    set = reinterpret_cast<Set>(
                        GetProcAddress(static_cast<HMODULE>(library), "cuCtxSetCurrent"));
#else
                    get = reinterpret_cast<Get>(dlsym(library, "cuCtxGetCurrent"));
                    set = reinterpret_cast<Set>(dlsym(library, "cuCtxSetCurrent"));
#endif
                    if (!get || !set) {
#if defined(_WIN32)
                        FreeLibrary(static_cast<HMODULE>(library));
#else
                        dlclose(library);
#endif
                        library = nullptr;
                        throw std::runtime_error("CUDA context symbols are unavailable");
                    }
                }

                ~Api() {
                    if (library)
#if defined(_WIN32)
                        FreeLibrary(static_cast<HMODULE>(library));
#else
                        dlclose(library);
#endif
                }
        };

        Api &api() {
            static Api value;
            return value;
        }

        void check(Result result, const char *operation) {
            if (result != 0)
                throw std::runtime_error(operation);
        }
    } // namespace

    Context current() {
        Context context = nullptr;
        check(api().get(&context), "cuCtxGetCurrent failed");
        if (!context)
            throw std::runtime_error("CUDA device did not establish a current context");
        return context;
    }

    Guard::Guard(Context context) {
        if (!context)
            return;
        Context active = nullptr;
        check(api().get(&active), "cuCtxGetCurrent failed");
        if (active != context)
            check(api().set(context), "cuCtxSetCurrent failed");
    }

    Guard::~Guard() noexcept = default;
} // namespace Mln::Cuda
