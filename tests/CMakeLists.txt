add_test(NAME hello_world_prints_greeting COMMAND hello_world)
set_tests_properties(hello_world_prints_greeting PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, world!"
)
