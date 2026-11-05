cmake_minimum_required(VERSION 3.20)
project(OrderBookEngine VERSION 0.3.0 LANGUAGES CXX)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Default build type is Release----
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler-specific flags 
if(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /utf-8
        /MP
        /wd4100
    )
    add_compile_options("$<$<CONFIG:Release>:/O2>")
    add_compile_options("$<$<CONFIG:Release>:/Oi>")
    add_compile_options("$<$<CONFIG:Release>:/Ot>")
    add_compile_options("$<$<CONFIG:Release>:/GL>")
    add_compile_options("$<$<CONFIG:Release>:/DNDEBUG>")
    add_link_options("$<$<CONFIG:Release>:/LTCG>")
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    add_compile_options("$<$<CONFIG:Release>:-O3>")
    add_compile_options("$<$<CONFIG:Release>:-march=native>")
    add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")
endif()

#Threading
find_package(Threads REQUIRED)

#Main engine 
add_executable(order_book_engine src/main.cpp)
target_include_directories(order_book_engine PRIVATE include)
target_link_libraries(order_book_engine PRIVATE Threads::Threads)

if(WIN32)
    set_target_properties(order_book_engine PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# test
add_executable(test_1 test/test_1.cpp)
target_include_directories(test_1 PRIVATE include)
add_executable(test_2 test/test_2.cpp)
target_include_directories(test_2 PRIVATE include)