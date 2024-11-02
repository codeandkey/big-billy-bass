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

static signalProcessor *processor;

void sigint_handler(int _sig)
{
    if (processor) {
        // processor->setState(STOPPED);
    } else {
        ERROR("Processor not running for SIGINT");
    }
}

int main(int argc, char **argv)
{
    float lpf = signalProcessingDefaults::LPF_CUTOFF_DEFAULT;
    float hpf = signalProcessingDefaults::HPF_CUTOFF_DEFAULT;
    uint64_t timeTag = 0;
    char fileName[255] = "test.mp3";

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
            INFO("HPF setting: %s", argv[i + 1]);
            i++;
        }
    }

    if (gpio::gpio_spawn() < 0) {
        ERROR("Failed spawning GPIO process");
        return -1;
    }

    audioDriver driver = audioDriver();
    audioFile file = audioFile(fileName, timeTag);

    processor = new signalProcessor();

    processor->setAudioDriver(&driver);
    processor->setLPF(lpf);
    processor->setHPF(hpf);
    processor->setFile(&file);

    do {
        processor->update(State::PLAYING);
    } while (processor->getState() != State::STOPPED);

    if (gpio::gpio_exit() < 0) {
        ERROR("Failed to exit GPIO process");
        return -1;
    }

    return 0;
}
