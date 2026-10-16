add_library(mln_package STATIC
  package.cpp
  metadata.cpp
)
target_include_directories(mln_package PUBLIC
  $<BUILD_INTERFACE:${MLN_ROOT}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(mln_package PUBLIC cxx_std_23)
set_target_properties(mln_package PROPERTIES EXPORT_NAME package)
add_library(MyeLiN::package ALIAS mln_package)

if(MLN_BUILD_FUZZERS)
  target_compile_options(mln_package PRIVATE -fsanitize=fuzzer-no-link)
  add_executable(mln_package_fuzzer fuzz/package_fuzzer.cpp)
  target_link_libraries(mln_package_fuzzer PRIVATE mln_package)
  target_compile_options(mln_package_fuzzer PRIVATE -fsanitize=fuzzer)
  target_link_options(mln_package_fuzzer PRIVATE -fsanitize=fuzzer)
endif()

install(
  TARGETS mln_package
  EXPORT MyeLiNTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(MLN_BUILD_RUNTIME)
  find_package(Threads REQUIRED)

if(MLN_NATIVE_ONLY_RUNTIME)
  add_library(mln_runtime SHARED
    native_only.cpp
    native_registry.cpp
    library.cpp
  )
  target_link_libraries(mln_runtime PUBLIC mln_package mln_sdk)
  target_compile_definitions(mln_runtime PRIVATE MLN_RUNTIME_BUILD=1)
  target_include_directories(mln_runtime PUBLIC
    $<BUILD_INTERFACE:${MLN_ROOT}/include>
    $<INSTALL_INTERFACE:include>
  )
else()
  add_library(mln_runtime SHARED
    c_api.cpp
    configuration.cpp
    runtime.cpp
    symbols.cpp
    native_registry.cpp
    driver_registry.cpp
    external_driver.cpp
    library.cpp
    cuda_context.cpp
  )
  target_link_libraries(mln_runtime PUBLIC mln_package mln_sdk)
  target_compile_definitions(mln_runtime PRIVATE MLN_RUNTIME_BUILD=1)

  # Headers publicos do MyeLiN.
  target_include_directories(mln_runtime PUBLIC
    $<BUILD_INTERFACE:${MLN_ROOT}/include>
    $<INSTALL_INTERFACE:include>
  )

  target_include_directories(mln_runtime PRIVATE
    ${MLN_ROOT}/src/sdk
    ${MLN_ROOT}/third_party/tomlplusplus/include
  )

  target_include_directories(mln_runtime SYSTEM PRIVATE
    ${MLN_ROOT}/third_party/iree/runtime/src
  )

  # IREE e um detalhe privado da implementacao do Runtime.
  target_link_libraries(mln_runtime PRIVATE
    iree_runtime_runtime
    iree_hal_hal
    iree_hal_utils_profile_file
    iree_io_formats_safetensors_safetensors
    iree_io_formats_irpa_irpa
    iree_io_formats_parser_registry
    iree_modules_io_parameters_parameters
    iree_io_parameter_index_provider
    iree_hal_drivers_local_task_task_driver
    iree_hal_local_local
    iree_task_api
  )

  # A ponte Wasm usa Wasmtime internamente.
  find_path(WASMTIME_INCLUDE_DIR wasmtime.hh REQUIRED)
  find_library(WASMTIME_LIB wasmtime REQUIRED)
  target_include_directories(mln_runtime SYSTEM PRIVATE ${WASMTIME_INCLUDE_DIR})
  target_link_libraries(mln_runtime PRIVATE
    ${WASMTIME_LIB}
    Threads::Threads
    ${CMAKE_DL_LIBS}
  )
endif()

  target_compile_features(mln_runtime PUBLIC cxx_std_23)
  set_target_properties(mln_runtime PROPERTIES
    EXPORT_NAME runtime
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )
  add_library(MyeLiN::runtime ALIAS mln_runtime)

  install(
    TARGETS mln_runtime
    EXPORT MyeLiNTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

if(MLN_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_executable(mln_weights_test tests/weights_test.cpp)
  target_include_directories(mln_weights_test PRIVATE ${MLN_ROOT}/include)
  target_link_libraries(mln_weights_test PRIVATE mln_sdk)
  target_compile_features(mln_weights_test PRIVATE cxx_std_23)

  add_executable(mln_package_test tests/package_test.cpp)
  target_link_libraries(mln_package_test PRIVATE mln_package)
  target_compile_features(mln_package_test PRIVATE cxx_std_23)

if(MLN_BUILD_RUNTIME)
  add_executable(mln_runtime_lifetime_test tests/runtime_lifetime_test.cpp)
  target_link_libraries(mln_runtime_lifetime_test PRIVATE mln_runtime)
  target_compile_features(mln_runtime_lifetime_test PRIVATE cxx_std_23)
  add_test(NAME mln_runtime_lifetime COMMAND mln_runtime_lifetime_test)

  add_executable(mln_c_api_test tests/c_api_test.c)
  target_link_libraries(mln_c_api_test PRIVATE mln_runtime)
  add_test(NAME mln_c_api COMMAND mln_c_api_test)

  add_executable(mln_runtime_concurrency_test tests/runtime_concurrency_test.cpp)
  target_link_libraries(mln_runtime_concurrency_test PRIVATE mln_runtime Threads::Threads)
  target_compile_features(mln_runtime_concurrency_test PRIVATE cxx_std_23)
  add_test(NAME mln_runtime_concurrency COMMAND mln_runtime_concurrency_test)

  add_library(mln_native_entrypoint_fixture MODULE tests/native_entrypoint_fixture.cpp)
  target_link_libraries(mln_native_entrypoint_fixture PRIVATE mln_sdk)
  target_compile_features(mln_native_entrypoint_fixture PRIVATE cxx_std_23)
  set_target_properties(mln_native_entrypoint_fixture PROPERTIES PREFIX "")

  add_executable(mln_native_artifact_test tests/native_artifact_test.cpp)
  target_link_libraries(mln_native_artifact_test PRIVATE mln_runtime)
  target_compile_features(mln_native_artifact_test PRIVATE cxx_std_23)
  add_test(NAME mln_native_artifact COMMAND mln_native_artifact_test
    $<TARGET_FILE:mln_native_entrypoint_fixture>
    ${CMAKE_CURRENT_BINARY_DIR}/native-only.mln)

endif()

add_test(NAME mln_weights COMMAND mln_weights_test)
add_test(NAME mln_package COMMAND mln_package_test
  ${CMAKE_CURRENT_BINARY_DIR}/test-assets.mln)
endif()
