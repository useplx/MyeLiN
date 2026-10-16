add_library(mln_sdk STATIC
  buffer.cpp
  session.cpp
  modules.cpp
  orchestrations.cpp
  operations.cpp
  parameters.cpp
  module.cpp
  tensor.cpp
  nn/linear.cpp
  nn/conv2d.cpp
  nn/silu.cpp
  nn/gelu.cpp
  audio/dsp.cpp
)

target_include_directories(mln_sdk PUBLIC
  $<BUILD_INTERFACE:${MLN_ROOT}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(mln_sdk PUBLIC cxx_std_23)

set_target_properties(mln_sdk PROPERTIES EXPORT_NAME sdk)
add_library(MyeLiN::sdk ALIAS mln_sdk)

install(
  TARGETS mln_sdk
  EXPORT MyeLiNTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(MLN_BUILD_TESTS)
  add_executable(mln_buffer_test tests/buffer_test.cpp)
  target_link_libraries(mln_buffer_test PRIVATE mln_sdk)
  target_compile_features(mln_buffer_test PRIVATE cxx_std_23)
  add_test(NAME mln_buffer COMMAND mln_buffer_test)

  add_executable(mln_diagnostic_test tests/diagnostic_test.cpp)
  target_link_libraries(mln_diagnostic_test PRIVATE mln_sdk)
  target_compile_features(mln_diagnostic_test PRIVATE cxx_std_23)
  add_test(NAME mln_diagnostic COMMAND mln_diagnostic_test)

  add_executable(mln_typed_registry_test tests/typed_registry_test.cpp)
  target_link_libraries(mln_typed_registry_test PRIVATE mln_sdk)
  target_compile_features(mln_typed_registry_test PRIVATE cxx_std_23)
  add_test(NAME mln_typed_registry COMMAND mln_typed_registry_test)

  add_executable(mln_operation_lifetime_test tests/operation_lifetime_test.cpp)
  target_link_libraries(mln_operation_lifetime_test PRIVATE mln_sdk)
  target_compile_features(mln_operation_lifetime_test PRIVATE cxx_std_23)
  add_test(NAME mln_operation_lifetime COMMAND mln_operation_lifetime_test)

  add_executable(mln_kind_test tests/kind_test.cpp)
  target_link_libraries(mln_kind_test PRIVATE mln_sdk)
  target_compile_features(mln_kind_test PRIVATE cxx_std_23)
  add_test(NAME mln_kind COMMAND mln_kind_test)

  add_executable(mln_audio_test tests/audio_test.cpp)
  target_link_libraries(mln_audio_test PRIVATE mln_sdk)
  target_compile_features(mln_audio_test PRIVATE cxx_std_23)
  foreach(audio_case window rms stft magnitude mel_filterbank logmel spectral_flux event_detector fingerprint classifier augmentation)
    add_test(NAME mln_audio_${audio_case} COMMAND mln_audio_test ${audio_case})
  endforeach()

  if(MLN_BUILD_RUNTIME)
    add_executable(mln_public_headers_test tests/public_headers_test.cpp)
    target_link_libraries(mln_public_headers_test PRIVATE mln_runtime)
    target_compile_features(mln_public_headers_test PRIVATE cxx_std_23)
    add_test(NAME mln_public_headers COMMAND mln_public_headers_test)
  endif()
endif()
