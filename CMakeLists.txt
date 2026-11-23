cmake_minimum_required(VERSION 3.16)

project(analog_devices_challenge C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(test_ringbuf
    part1/spsc_ringbuf.c
    part1/test_ringbuf.c
)
target_include_directories(test_ringbuf PRIVATE part1)
target_link_libraries(test_ringbuf PRIVATE Threads::Threads)

add_executable(test_pi_mutex
    part1/pi_mutex.c
    part1/test_pi_mutex.c
)
target_include_directories(test_pi_mutex PRIVATE part2)
target_link_libraries(test_pi_mutex PRIVATE Threads::Threads)

enable_testing()
add_test(NAME ringbuf COMMAND test_ringbuf)
add_test(NAME pi_mutex COMMAND test_pi_mutex)
