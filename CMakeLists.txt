cmake_minimum_required(VERSION 3.6)
project(shodantask)

# The 'COMPONENTS' argument should only list libraries which
# require linking (not header-only).
find_package(Boost 1.60 COMPONENTS coroutine)

add_executable(shodantask
    src/Helpers.hpp
    src/MemoryMappedFile.cpp
    src/MemoryMappedFile.hpp
    src/RegexSearchFile.cpp
    src/RegexSearchFile.hpp
    src/main.cpp
    README.txt)

target_link_libraries(shodantask
    Boost::boost      # <- For header-only libraries.
    Boost::coroutine)

set_target_properties(shodantask PROPERTIES
    CXX_STANDARD_REQUIRED FALSE
    CXX_STANDARD          11)
