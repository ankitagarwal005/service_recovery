cmake_minimum_required(VERSION 3.16)
project(service_recovery_scheduler
        VERSION 1.0.0
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings
add_compile_options(
    -Wall -Wextra -Wpedantic -Wshadow
    -Wnon-virtual-dtor -Wold-style-cast
    -Woverloaded-virtual -Wnull-dereference
)

# ── Library ────────────────────────────────────────────────────────────────
add_library(srs_lib STATIC
    src/sched.cpp
)
target_include_directories(srs_lib PUBLIC include)

# ── Main executable ────────────────────────────────────────────────────────
add_executable(srs_demo src/main.cpp)
target_link_libraries(srs_demo PRIVATE srs_lib)

# ── Tests ──────────────────────────────────────────────────────────────────
enable_testing()

add_executable(srs_tests tests/test_sched.cpp)
target_include_directories(srs_tests PRIVATE tests)
target_link_libraries(srs_tests PRIVATE srs_lib)

add_test(NAME unit_tests COMMAND srs_tests)