cmake_minimum_required(VERSION 3.6)
project(shodantask)

add_executable(shodan-task
    src/Helpers.hpp
    src/MemoryMappedFile.cpp
    src/MemoryMappedFile.hpp
    src/RegexSearchFile.cpp
    src/RegexSearchFile.hpp
    src/main.cpp
    README.txt)

target_link_libraries(shodan-task -lboost_system -lboost_coroutine)

set_target_properties(shodan-task PROPERTIES
    CXX_STANDARD_REQUIRED FALSE
    CXX_STANDARD          11)
