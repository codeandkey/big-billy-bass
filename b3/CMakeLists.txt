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

if (USE_PCM_S16)
    message(STATUS "PCM resolution set to signed 16-bit int")
elseif(USE_PCM_S32)
    message(STATUS "PCM resolution set to signed 32-bit int")
elseif(USE_PCM_F32)
    message(STATUS "PCM resolution set to 32-bit float")
elseif(USE_PCM_F64)
    message(STATUS "PCM resolution set to 64-bit float")
else()
    message(STATUS "PCM resolution set to signed 16-bit int")
endif()
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
