set(NAPI_V8_V8_MONOLITH_LIB_DEFAULT "")
if(DEFINED ENV{NAPI_V8_V8_LIBRARY})
  set(NAPI_V8_V8_MONOLITH_LIB_DEFAULT "$ENV{NAPI_V8_V8_LIBRARY}")
elseif(DEFINED ENV{NAPI_V8_V8_MONOLITH_LIB})
  set(NAPI_V8_V8_MONOLITH_LIB_DEFAULT "$ENV{NAPI_V8_V8_MONOLITH_LIB}")
endif()
set(NAPI_V8_V8_MONOLITH_LIB
  "${NAPI_V8_V8_MONOLITH_LIB_DEFAULT}"
  CACHE FILEPATH "Path to V8 runtime library (monolith or dylib)"
)
set(NAPI_V8_V8_EXTRA_LIBS_DEFAULT "")
if(DEFINED ENV{NAPI_V8_V8_EXTRA_LIBS})
  set(NAPI_V8_V8_EXTRA_LIBS_DEFAULT "$ENV{NAPI_V8_V8_EXTRA_LIBS}")
endif()
set(NAPI_V8_V8_EXTRA_LIBS
  "${NAPI_V8_V8_EXTRA_LIBS_DEFAULT}"
  CACHE STRING "Semicolon-separated extra V8 libraries (e.g. libplatform/libbase)"
)
set(NAPI_V8_GTEST_ROOT "${NAPI_V8_NODE_ROOT}/deps/googletest")
include(GoogleTest)

if(NAPI_V8_V8_MONOLITH_LIB)
  add_executable(napi_v8_tier1_tests
    "${NAPI_V8_GTEST_ROOT}/src/gtest-all.cc"
    "${NAPI_V8_GTEST_ROOT}/src/gtest_main.cc"
    tier1_ported_runner.cc
    ported/js-native-api/2_function_arguments/2_function_arguments.c
    ported/js-native-api/3_callbacks/3_callbacks.c
    ported/js-native-api/4_object_factory/4_object_factory.c
    ported/js-native-api/5_function_factory/5_function_factory.c
  )

  target_include_directories(napi_v8_tier1_tests
    PRIVATE
      "${NAPI_V8_GTEST_ROOT}"
      "${NAPI_V8_GTEST_ROOT}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/ported/js-native-api"
      "${NAPI_V8_ROOT}/include"
      "${NAPI_V8_V8_INCLUDE_DIR}"
  )

  target_link_libraries(napi_v8_tier1_tests PRIVATE napi_v8)
  target_link_libraries(napi_v8_tier1_tests PRIVATE "${NAPI_V8_V8_MONOLITH_LIB}")
  if(NAPI_V8_V8_DEFINES)
    target_compile_definitions(napi_v8_tier1_tests PRIVATE ${NAPI_V8_V8_DEFINES})
  endif()
  if(NAPI_V8_V8_EXTRA_LIBS)
    target_link_libraries(napi_v8_tier1_tests PRIVATE ${NAPI_V8_V8_EXTRA_LIBS})
  endif()
  get_filename_component(NAPI_V8_V8_LIB_DIR "${NAPI_V8_V8_MONOLITH_LIB}" DIRECTORY)
  set_target_properties(napi_v8_tier1_tests PROPERTIES
    BUILD_RPATH "${NAPI_V8_V8_LIB_DIR}"
    INSTALL_RPATH "${NAPI_V8_V8_LIB_DIR}"
  )
  gtest_discover_tests(
    napi_v8_tier1_tests
    DISCOVERY_TIMEOUT 30
    TEST_PREFIX "napi_v8."
  )
else()
  message(STATUS "Skipping napi_v8_tier1_tests: set NAPI_V8_V8_MONOLITH_LIB to link V8 runtime")
endif()
