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

using namespace b3;

static int shouldExit;

void sigint_handler(int _sig)
{
    INFO("SIGINT received, shutting down..");
    shouldExit = 1;
}

int main(int argc, char **argv)
{
    float lpf = signalProcessingDefaults::LPF_CUTOFF_DEFAULT;
    float hpf = signalProcessingDefaults::HPF_CUTOFF_DEFAULT;
    uint64_t timeTag = 0;
    char fileName[255] = "test.mp3";
    int mouthMinRMS = 0, bodyMinRMS = 10000;

    gpio::GpioConfig gpioConfig;

    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "-v" || std::string(argv[i]) == "--verbose") {
            SET_VERBOSE_LOGGING(true);
            INFO("Verbose logging enabled");
        }
        if (std::string(argv[i]) == "-f" && i + 1 < argc) {
            strncpy(fileName, argv[i + 1], sizeof(fileName));
            INFO("loading sound file: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-lpf" && i + 1 < argc) {
            lpf = std::stod(argv[i + 1]);
            INFO("LPF setting: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-hpf" && i + 1 < argc) {
            lpf = std::stod(argv[i + 1]);
            INFO("HPF setting: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-seek" && i + 1 < argc) {
            timeTag = std::stod(argv[i + 1]);
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
    audioFile file = audioFile(fileName, timeTag);
    signalProcessor processor = signalProcessor();

    processor.setAudioDriver(&driver);
    processor.setLPF(lpf);
    processor.setHPF(hpf);
    processor.setFile(&file);

    do {
        processor.update(State::PLAYING);
    } while (!shouldExit && processor.getState() != State::STOPPED);

    INFO("Shutting down...");

    if (gpio::gpio_exit() < 0) {
        ERROR("Failed to terminate GPIO");
    }

    return 0;
}
