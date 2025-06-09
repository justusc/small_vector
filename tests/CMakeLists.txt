# tests/CMakeLists.txt

enable_testing()

include(GoogleTest)

set(TEST_NAME small_vector)

set_target_properties(gtest PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(gtest_main PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(gmock PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(gmock_main PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
set_target_properties(check PROPERTIES EXCLUDE_FROM_ALL TRUE)

foreach(cpp_standard IN ITEMS 11 14 17 20 23)
  add_executable(
    ${TEST_NAME}_test_cpp${cpp_standard}
    main_test.cc
    small_vector_test.cc
  )
  target_link_libraries(
    ${TEST_NAME}_test_cpp${cpp_standard}
    PRIVATE
    small_vector # the library
    GTest::gtest_main
  )
  target_compile_options(${TEST_NAME}_test_cpp${cpp_standard} PRIVATE -Wall -Wextra -Werror -pedantic)

  target_compile_features(${TEST_NAME}_test_cpp${cpp_standard} PRIVATE cxx_std_${cpp_standard})
  set_target_properties(${TEST_NAME}_test_cpp${cpp_standard}
    PROPERTIES 
    CXX_EXTENSIONS OFF
    EXCLUDE_FROM_ALL TRUE)

  gtest_discover_tests(${TEST_NAME}_test_cpp${cpp_standard})
  add_dependencies(check ${TEST_NAME}_test_cpp${cpp_standard})
  
endforeach()

