add_test(NAME hello_world_prints_greeting COMMAND hello_world)
set_tests_properties(hello_world_prints_greeting PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, world!"
)

add_executable(test_stm test_stm.c)
target_link_libraries(test_stm PRIVATE stm)

target_compile_options(test_stm PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

add_test(NAME stm_tests COMMAND test_stm)
