#include <cstdio>
#include <chrono>
#include <cstring>
#include <string>

extern "C" {
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
}

#include "gpio.h"
#include "logger.h"
#include "signalProcessing.h"
#include "audioDriver.h"
#include "audioFile.h"
#include "b3Config.h"

using namespace b3;

static int shouldExit;

void sigint_handler(int _sig)
{
    INFO("SIGINT received, shutting down..");
    shouldExit = 1;
}

int main(int argc, char **argv)
{
    uint64_t seekTime = 0;
    char fileName[255];
    snprintf(fileName, sizeof(fileName), "%s/%s", audioFileDefaults::AUDIO_FILES_PATH, audioFileDefaults::DEFAULT_FILE_NAME);
    int mouthMinRMS = 0, bodyMinRMS = 10000;

    gpio::GpioConfig gpioConfig;
    b3Config globalConfig;

    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "-v" || std::string(argv[i]) == "--verbose") {
            SET_VERBOSE_LOGGING(true);
            INFO("Verbose logging enabled");
        }
        if (std::string(argv[i]) == "-f" && i + 1 < argc) {
            snprintf(fileName, sizeof(fileName), "%s/%s", audioFileDefaults::AUDIO_FILES_PATH, argv[i + 1]);
            INFO("loading sound file: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-lpf" && i + 1 < argc) {
            globalConfig.LPF_CUTOFF = std::stod(argv[i + 1]);
            INFO("LPF setting: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-hpf" && i + 1 < argc) {
            globalConfig.HPF_CUTOFF = std::stod(argv[i + 1]);
            INFO("HPF setting: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-seek" && i + 1 < argc) {
            seekTime = std::stod(argv[i + 1]);
            INFO("Seeking to +%s seconds", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-body" && i + 1 < argc) {
            gpioConfig.m_bodyMinRMS = std::stoi(argv[i + 1]);
            INFO("Body RMS threshold %d", gpioConfig.m_bodyMinRMS);
            i++;
        }
        if (std::string(argv[i]) == "-mouth" && i + 1 < argc) {
            gpioConfig.m_mouthMinRMS = std::stoi(argv[i + 1]);
            INFO("Mouth RMS threshold %d", gpioConfig.m_mouthMinRMS);
            i++;
        }
    }

    if (gpio::gpio_spawn(gpioConfig) < 0) {
        ERROR("Failed spawning GPIO thread");
        return -1;
    }

    signal(SIGINT, sigint_handler);


    audioDriver driver = audioDriver();
    audioFile file = audioFile(fileName, seekTime);
    signalProcessor processor = signalProcessor(globalConfig);

    processor.setAudioDriver(&driver);
    processor.setFile(&file);

    do {
        globalConfig.poll();
        processor.setLPF(globalConfig.LPF_CUTOFF);
        processor.setHPF(globalConfig.HPF_CUTOFF);
        processor.update(State::PLAYING);
    } while (!shouldExit && processor.getState() != State::STOPPED);

    INFO("Shutting down...");

    if (gpio::gpio_exit() < 0) {
        ERROR("Failed to terminate GPIO");
    }

    return 0;
}
