cmake_minimum_required (VERSION 3.5)
project (BigBillyBass)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_BUILD_TYPE Debug)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-gnu-zero-variadic-macro-arguments")

# In the event you have a broken xcode toolchain
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
    #set (CMAKE_CXX_COMPILER g++-14)
    #set (CMAKE_C_COMPILER gcc-14)
endif()

# Comment this line to disable GPIO
set (ENABLE_GPIO 1)

# Comment this line to disable ALSA output
set (ENABLE_ASOUND 1)

add_subdirectory (b3)
