cmake_minimum_required(VERSION 3.16)
project(embedded_interview_c LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(interview STATIC interview.c)
target_include_directories(interview PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(interview_tests test_main.c)
target_link_libraries(interview_tests PRIVATE interview)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(interview PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow)
    target_compile_options(interview_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow)
elseif(MSVC)
    target_compile_options(interview PRIVATE /W4)
    target_compile_options(interview_tests PRIVATE /W4)
endif()

enable_testing()
add_test(NAME interview_boundaries COMMAND interview_tests)
