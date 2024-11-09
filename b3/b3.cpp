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
#include "sighandler.h"

using namespace b3;
using namespace std;



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

    globalConfig.printSettings();

    GPIO gpio = GPIO(&globalConfig);
    gpio.start(signalHandler::sigintHandler);

    audioDriver driver = audioDriver();
    audioFile file = audioFile();
    signalProcessor processor = signalProcessor(globalConfig);

    if (file.openFile(fileName, seekTime) != 0){
        INFO("Failed to open %s, exiting...",fileName);
        return -1;
    }
    processor.setAudioDriver(&driver);
    processor.setFile(&file);

    do {
        globalConfig.poll();
        processor.update(State::PLAYING);
    } while (!signalHandler::g_shouldExit && processor.getState() != State::STOPPED);

    INFO("Shutting down...");

    globalConfig.SEEK_TIME = file.getCurrentTimestampUs();
    globalConfig.printSettings();

    gpio.stop();

    DEBUG("Have a nice day :)");
    return 0;
}
