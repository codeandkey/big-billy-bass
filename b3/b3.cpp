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
    b3Config globalConfig;
    // read cfg file
    globalConfig.poll();
    // override settings with any cmd line args
    globalConfig.parse_cmd_args(argc, argv);
    // save settings 
    globalConfig.print_settings();



    signalProcessor processor = signalProcessor(globalConfig);
    audioDriver *driver = new audioDriver();
    audioSource *source = nullptr;

    switch (globalConfig.PROGRAM_OP_MODE) {

    case op_mode::bluetooth:
        INFO("Operating in bluetooth mode");
        break;
    case op_mode::file_based:
        INFO("Operating in file-based mode");
        audioFile *file = new audioFile();
        if (file->open_file(globalConfig.ACTIVE_FILE, globalConfig.SEEK_TIME) != 0) {
            INFO("Failed to open %s, exiting...", globalConfig.ACTIVE_FILE);
            return -1;
        }
        source = file;
        break;
    }
    if (!source) {
        ERROR("No Audio Source could be initialized, shutting down");
        return -1;
    }
    processor.set_audio_driver(driver);
    processor.set_audio_source(source);

    GPIO gpio = GPIO(&globalConfig);
    gpio.start(signalHandler::sigintHandler);
    do {
        globalConfig.poll();
        processor.update(State::PLAYING);
    } while (!signalHandler::g_shouldExit && processor.get_state() != State::STOPPED);


    INFO("Shutting down...");
    globalConfig.SEEK_TIME = source->stream_timestamp_uS();
    globalConfig.print_settings();
    gpio.stop();

    DEBUG("Have a nice day :)");
    return 0;
}