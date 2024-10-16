add_executable(b3
    b3.cpp
    task.cpp
    gpioStream.cpp
    signalProcessing.cpp
    logger.cpp
    timeManager.cpp
    biQuadFilter.cpp
    audioDriver.cpp
    audioFile.cpp
)

# needed for ffmpeg libs
target_link_libraries(b3 
    avformat
    avcodec
    asound
    avutil
    swresample
)

# Uncomment to enable pigpio
set (PIGPIO_ENABLE 1)

# Uncomment to enable GPIO test
# set (GPIO_TEST 1)

if (PIGPIO_ENABLE)
    target_compile_definitions(b3 PUBLIC ENABLE_PIGPIO)
    target_link_libraries(b3 pigpio)
endif()

if (GPIO_TEST)
    target_compile_definitions(b3 PUBLIC GPIO_TEST)
endif()

# Comment to disable audio override
target_compile_definitions(b3 PUBLIC TEST_AUDIO="test.mp3")
