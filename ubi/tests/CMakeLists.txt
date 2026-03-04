include(GoogleTest)

set(NAPI_V8_GTEST_ROOT "${PROJECT_ROOT}/deps/googletest")
set(UBI_GTEST_PER_TEST_TIMEOUT_SECONDS 60)
set(UBI_CTEST_RESOURCE_LOCK_BUCKETS "24" CACHE STRING
    "Max concurrent ubi gtest cases when ctest parallelism is unbounded (0 disables).")
set(UBI_CTEST_RESOURCE_LOCK_PREFIX "ubi_gtest_slot" CACHE STRING
    "Prefix used for generated CTest RESOURCE_LOCK buckets.")

if(TARGET napi_v8 AND NAPI_V8_LIBRARY)
  add_library(ubi_gtest STATIC
    "${NAPI_V8_GTEST_ROOT}/src/gtest-all.cc"
    "${NAPI_V8_GTEST_ROOT}/src/gtest_main.cc"
  )

  target_include_directories(ubi_gtest
    PRIVATE
      "${NAPI_V8_GTEST_ROOT}"
      "${NAPI_V8_GTEST_ROOT}/include"
      "${UBI_ROOT}/tests/runners"
      "${UBI_ROOT}/src"
      "${NAPI_INCLUDE_ROOT}"
  )

  function(add_ubi_node_api_test target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name}
      PRIVATE
        "${NAPI_V8_GTEST_ROOT}"
        "${NAPI_V8_GTEST_ROOT}/include"
        "${UBI_ROOT}/tests/runners"
        "${UBI_ROOT}/src"
        "${NAPI_INCLUDE_ROOT}"
    )
    target_link_libraries(${target_name}
      PRIVATE
        ubi_gtest
        napi_v8
        ubi_node_api
        ubi_runtime
        ubi_cli
    )
    target_compile_definitions(${target_name}
      PRIVATE
        NAPI_V8_NODE_ROOT_PATH="${NAPI_V8_NODE_ROOT}"
        PROJECT_ROOT_PATH="${PROJECT_ROOT}"
        NAPI_V8_ROOT_PATH="${UBI_ROOT}"
    )
    if(NAPI_V8_DEFINES)
      target_compile_definitions(${target_name} PRIVATE ${NAPI_V8_DEFINES})
    endif()
    # Keep a stable path to the generated CTest manifest for this target.
    set(_gtest_tests_manifest "${CMAKE_CURRENT_BINARY_DIR}/${target_name}[1]_tests.cmake")
    gtest_discover_tests(
      ${target_name}
      DISCOVERY_TIMEOUT 30
      TEST_PREFIX "ubi.${target_name}."
      PROPERTIES
        TIMEOUT ${UBI_GTEST_PER_TEST_TIMEOUT_SECONDS}
    )
    # CMake 3.31 can emit duplicated add_test entries for very large gtest lists.
    # Deduplicate generated manifest entries so each gtest runs once in ctest.
    add_custom_command(
      TARGET ${target_name}
      POST_BUILD
      COMMAND /usr/bin/env python3
              ${UBI_ROOT}/scripts/dedupe_gtest_manifest.py
              ${_gtest_tests_manifest}
              --resource-lock-buckets ${UBI_CTEST_RESOURCE_LOCK_BUCKETS}
              --resource-lock-prefix ${UBI_CTEST_RESOURCE_LOCK_PREFIX}
      VERBATIM
    )
  endfunction()

  add_ubi_node_api_test(ubi_test_0_runtime_phase01
    "${UBI_ROOT}/tests/runners/test_0_runtime_phase01.cc"
  )
  add_ubi_node_api_test(ubi_test_1_cli_phase01
    "${UBI_ROOT}/tests/runners/test_1_cli_phase01.cc"
  )
  add_ubi_node_api_test(ubi_test_2_module_loader_phase02
    "${UBI_ROOT}/tests/runners/test_2_module_loader_phase02.cc"
  )
  add_ubi_node_api_test(ubi_test_3_node_dropin_subset_phase02
    "${UBI_ROOT}/tests/runners/test_3_node_dropin_subset_phase02.cc"
  )
  add_ubi_node_api_test(ubi_test_4_internal_binding_dispatch_phase02
    "${UBI_ROOT}/tests/runners/test_4_internal_binding_dispatch_phase02.cc"
  )
  add_ubi_node_api_test(ubi_test_5_internal_binding_parity_phase03
    "${UBI_ROOT}/tests/runners/test_5_internal_binding_parity_phase03.cc"
  )
else()
  message(STATUS "Skipping ubi node-api tests: napi_v8 runtime target/linkage is unavailable")
endif()

add_test(
  NAME ubi.guard_internal_binding_dispatch
  COMMAND /usr/bin/env python3
          "${UBI_ROOT}/scripts/check_internal_binding_dispatch.py"
          "${UBI_ROOT}/src/ubi_module_loader.cc"
)
