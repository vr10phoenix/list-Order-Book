# cmake_minimum_required(VERSION 3.20)
# project(Avangard VERSION 0.1.0 LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# if (MSVC)
#     add_compile_options(/O2 /W4 /permissive- /EHsc)
# else()
#     add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic -Wno-unused-parameter)
# endif()
# add_executable(demo src/main.cpp)
# target_include_directories(demo PRIVATE include)

# find_package(Threads REQUIRED)
# target_link_libraries(demo PRIVATE Threads::Threads)


cmake_minimum_required(VERSION 3.20)
project(Avangard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(demo src/main.cpp)
target_include_directories(demo PRIVATE include)

if (MSVC)
    target_compile_options(demo PRIVATE
        "$<$<CONFIG:Release>:/O2;/W4;/permissive-;/EHsc>"
        "$<$<CONFIG:Debug>:/W4;/permissive-;/EHsc>"
    )
else()
    target_compile_options(demo PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
        "$<$<CONFIG:Release>:-O3;-march=native>"
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(demo PRIVATE Threads::Threads)
