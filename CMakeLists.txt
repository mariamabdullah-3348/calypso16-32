cmake_minimum_required(VERSION 3.16)
project(calypso16_32_emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(calypso16_32
    main.cpp
    decode.cpp
    disassembler.cpp
    execute.cpp
    fetch.cpp
    m_extension.cpp
    memory.cpp
    self_tests.cpp
)

if (MSVC)
    target_compile_options(calypso16_32 PRIVATE /W4)
else()
    target_compile_options(calypso16_32 PRIVATE -Wall -Wextra -pedantic)
endif()
