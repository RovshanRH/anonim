cmake_minimum_required(VERSION 3.20)
project(anonim_backend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(anonim_server
    src/main.cpp
    src/chat_server.cpp
)

target_include_directories(anonim_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(anonim_server PRIVATE /W4 /permissive-)
else()
    target_compile_options(anonim_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(anonim_server PRIVATE ws2_32)
endif()
