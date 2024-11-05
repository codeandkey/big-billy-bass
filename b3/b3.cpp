#include <string>

extern "C" {
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
}

#include "gpio.h"
#include "logger.h"
#include "signalProcessing.h"
#include "audioDriver.h"
#include "audioFile.h"
#include "b3Config.h"

using namespace b3;
using namespace std;

volatile int shouldExit;

void sigintHandler(int sig)
{
    if (sig == SIGINT) {
        INFO("SIGINT received, shutting down..");
        shouldExit = 1;
    }
}

int main(int argc, char **argv)
{
    uint64_t seekTime = 0;
    char fileName[255];
    snprintf(fileName, sizeof(fileName), "%s/%s", audioFileDefaults::AUDIO_FILES_PATH, audioFileDefaults::DEFAULT_FILE_NAME);

    b3Config globalConfig;

    for (int i = 0; i < argc; ++i) {
        if (string(argv[i]) == "-v" || string(argv[i]) == "--verbose") {
            SET_VERBOSE_LOGGING(true);
            INFO("Verbose logging enabled");
        }
        if (string(argv[i]) == "-f" && i + 1 < argc) {
            snprintf(fileName, sizeof(fileName), "%s/%s", audioFileDefaults::AUDIO_FILES_PATH, argv[i + 1]);
            INFO("loading sound file: %s", argv[i + 1]);
            i++;
        }
        if (string(argv[i]) == "-lpf" && i + 1 < argc) {
            globalConfig.LPF_CUTOFF = stod(argv[i + 1]);
            INFO("LPF setting: %s", argv[i + 1]);
            i++;
        }
        if (string(argv[i]) == "-hpf" && i + 1 < argc) {
            globalConfig.HPF_CUTOFF = stod(argv[i + 1]);
            INFO("HPF setting: %s", argv[i + 1]);
            i++;
        }
        if (string(argv[i]) == "-seek" && i + 1 < argc) {
            seekTime = stod(argv[i + 1]);
            INFO("Seeking to +%s seconds", argv[i + 1]);
            i++;
        }
        if (string(argv[i]) == "-body" && i + 1 < argc) {
            globalConfig.BODY_THRESHOLD = stoi(argv[i + 1]);
            INFO("Body RMS threshold %d", globalConfig.BODY_THRESHOLD);
            i++;
        }
        if (string(argv[i]) == "-mouth" && i + 1 < argc) {
            globalConfig.MOUTH_THRESHOLD = stoi(argv[i + 1]);
            INFO("Mouth RMS threshold %d", globalConfig.MOUTH_THRESHOLD);
            i++;
        }
    }

    GPIO gpio = GPIO(&globalConfig);
    gpio.start(sigintHandler);

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

    gpio.stop();
    return 0;
}
