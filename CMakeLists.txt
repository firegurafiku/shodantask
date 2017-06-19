
add_executable(shodan-task
    MemoryMappedFile.hpp
    MemoryMappedFile.cpp
    main2.cpp
    README.txt)

set_target_properties(shodan-task PROPERTIES
    CXX_STANDARD_REQUIRED FALSE
    CXX_STANDARD          11)
