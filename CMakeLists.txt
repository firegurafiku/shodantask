
add_executable(shodan-task
    MemoryMappedFile.hpp
    MemoryMappedFile.cpp
    MMappedUrlExtractor.hpp
    BufferedUrlExtractor.hpp
    main2.cpp
    README.txt)

target_link_libraries(shodan-task -lboost_context)

set_target_properties(shodan-task PROPERTIES
    CXX_STANDARD_REQUIRED FALSE
    CXX_STANDARD          14)
