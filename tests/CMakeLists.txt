add_test(NAME hello_world_prints_greeting COMMAND hello_world)
set_tests_properties(hello_world_prints_greeting PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, world!"
)

add_executable(test_y_stm test_y_stm.c)
target_link_libraries(test_y_stm PRIVATE y_stm)

target_compile_options(test_y_stm PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

add_test(NAME y_stm_tests COMMAND test_y_stm)

add_executable(test_y_string test_y_string.c)
target_link_libraries(test_y_string PRIVATE y_string)

target_compile_options(test_y_string PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

add_test(NAME y_string_tests COMMAND test_y_string)
