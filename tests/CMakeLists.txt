add_executable(minerby_tests
  test_main.cpp
  test_sha256.cpp
  test_hex.cpp
  test_target.cpp
  test_stratum.cpp
)
target_link_libraries(minerby_tests PRIVATE minerby_core doctest::doctest)
if(MSVC)
  target_compile_options(minerby_tests PRIVATE /EHsc)
endif()

add_test(NAME minerby_tests COMMAND minerby_tests)
