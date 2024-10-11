add_executable(b3
    b3.cpp
    task.cpp
    gpioStream.cpp
    signalProcessing.cpp
    logger.cpp
    timeManager.cpp
)

target_link_libraries(b3 avformat)

# Comment to enable pigpio
#set (PIGPIO_ENABLE 1)

if (PIGPIO_ENABLE)
    target_compile_definitions(b3 PUBLIC ENABLE_PIGPIO)
    target_link_libraries(b3 pigpio)
else()
endif()

# Comment to disable audio override
target_compile_definitions(b3 PUBLIC TEST_AUDIO="test.mp3")
