cmake_minimum_required(VERSION 3.20)
project(anonim_backend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ANONIM_BUILD_TESTS "Build backend unit tests" ON)

find_package(Threads REQUIRED)

add_library(anonim_backend_core STATIC
    src/chat_server.cpp
    src/request_utils.cpp
)

target_include_directories(anonim_backend_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(anonim_backend_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(anonim_backend_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(anonim_backend_core PUBLIC ws2_32)
endif()

target_link_libraries(anonim_backend_core PUBLIC Threads::Threads)

add_executable(anonim_server
    src/main.cpp
)

target_link_libraries(anonim_server PRIVATE anonim_backend_core)

if(ANONIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
