include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)

FetchContent_MakeAvailable(Catch2)

add_executable(anonim_tests
    test_request_utils.cpp
    test_chat_server.cpp
)

target_compile_definitions(anonim_tests PRIVATE ANONIM_TESTING)

target_include_directories(anonim_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(anonim_tests
    PRIVATE
    anonim_backend_core
        Catch2::Catch2WithMain
)

set_target_properties(anonim_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

include(CTest)
include(Catch)
catch_discover_tests(anonim_tests)

add_subdirectory(scenarios)