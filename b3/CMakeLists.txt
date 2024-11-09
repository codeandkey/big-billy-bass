add_executable(b3
    b3.cpp
    gpio.cpp
    signalProcessing.cpp
    logger.cpp
    timeManager.cpp
    biQuadFilter.cpp
    audioDriver.cpp
    audioFile.cpp
    b3Config.cpp
    sighandler.cpp
)

# needed for ffmpeg libs
target_link_libraries(b3 
    avformat
    avcodec
    avutil
    swresample
)

if (ENABLE_ASOUND)
    message(STATUS "Enabling ALSA audio driver")
    target_link_libraries(b3 asound)
else()
    message(STATUS "Disabling ALSA audio driver")
    target_compile_definitions(b3 PUBLIC DUMMY_ALSA_DRIVERS)
endif()

if (ENABLE_GPIO)
    message(STATUS "Enabling GPIO")
    target_compile_definitions(b3 PUBLIC ENABLE_GPIO)
    target_link_libraries(b3 pigpio)
else()
    message(STATUS "Disabling GPIO")
    target_compile_definitions(b3 PUBLIC DISABLE_GPIO)
endif()
