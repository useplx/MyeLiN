cmake_minimum_required(VERSION 3.21)
project(myelin VERSION 1.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(MLN_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

option(MLN_BUILD_RUNTIME "Build the target .mln runtime" ON)
option(MLN_NATIVE_ONLY_RUNTIME "Build the compact native-provider-only runtime" OFF)
option(MLN_BUILD_TESTS "Build Myelin tests" ON)
option(MLN_ENABLE_CUDA "Enable the IREE CUDA runtime driver" OFF)
option(MLN_ENABLE_VULKAN "Enable the IREE Vulkan runtime driver" OFF)
option(MLN_ENABLE_HIP "Enable the IREE HIP runtime driver" OFF)
option(MLN_ENABLE_METAL "Enable the IREE Metal runtime driver" OFF)
option(MLN_ENABLE_RUNTIME_BACKTRACES
  "Fetch/build libbacktrace for native runtime stack traces" ON)
option(MLN_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MLN_BUILD_FUZZERS "Build libFuzzer targets for untrusted inputs" OFF)
option(MLN_BUILD_REFLEX_TOOLS
  "Build the official IREE profile and replay tools used by Reflex" OFF)
option(MLN_BUILD_REFLEX_BENCHMARKS
  "Build the official IREE replay benchmark tool" OFF)

if(MLN_BUILD_TESTS)
  enable_testing()
endif()

if(MLN_BUILD_RUNTIME AND NOT MLN_NATIVE_ONLY_RUNTIME)
  set(IREE_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
  set(IREE_BUILD_RUNTIME ON CACHE BOOL "" FORCE)
  set(IREE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(IREE_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(IREE_BUILD_BENCHMARKS ${MLN_BUILD_REFLEX_BENCHMARKS} CACHE BOOL "" FORCE)
  set(IREE_ENABLE_LIBBACKTRACE ${MLN_ENABLE_RUNTIME_BACKTRACES} CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_DEFAULTS OFF CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_CUDA ${MLN_ENABLE_CUDA} CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_HIP ${MLN_ENABLE_HIP} CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_LOCAL_SYNC ON CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_LOCAL_TASK ON CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_VULKAN ${MLN_ENABLE_VULKAN} CACHE BOOL "" FORCE)
  set(IREE_HAL_DRIVER_METAL ${MLN_ENABLE_METAL} CACHE BOOL "" FORCE)

  add_subdirectory(third_party/iree EXCLUDE_FROM_ALL)

  if(MLN_BUILD_REFLEX_TOOLS)
    add_custom_target(mln_reflex_tools ALL
      DEPENDS iree-profile iree-dump-replay iree-run-replay)
    if(MLN_BUILD_REFLEX_BENCHMARKS)
      add_dependencies(mln_reflex_tools iree-benchmark-replay)
    endif()
  endif()
endif()

if(MSVC)
  set(MLN_WARNINGS /W4)
else()
  set(MLN_WARNINGS -Wall -Wextra -Wshadow)
endif()
add_compile_options(${MLN_WARNINGS})

if(MLN_ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

if(MLN_BUILD_FUZZERS AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "MLN_BUILD_FUZZERS requires Clang with libFuzzer")
endif()

add_subdirectory(src/sdk)
add_subdirectory(src/runtime)

install(
    DIRECTORY include/mln
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT MyeLiNTargets
    FILE MyeLiNTargets.cmake
    NAMESPACE MyeLiN::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyeLiN
)

configure_package_config_file(
    cmake/MyeLiNConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MyeLiNConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyeLiN
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MyeLiNConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/MyeLiNConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/MyeLiNConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyeLiN
)
