cmake_minimum_required(VERSION 3.6)
project(shodantask)

add_executable(shodan-task
    MemoryMappedFile.hpp
    MemoryMappedFile.cpp
    RegexSearchFile.hpp
    RegexSearchFile.cpp
    Helpers.hpp
    main2.cpp
    README.txt)

target_link_libraries(shodan-task -lboost_system -lboost_coroutine)

set_target_properties(shodan-task PROPERTIES
    CXX_STANDARD_REQUIRED FALSE
    CXX_STANDARD          11)
