
#include <csignal>

#include "logger.h"
#include "sighandler.h"


volatile int signalHandler::g_shouldExit = 0;

void signalHandler::sigintHandler(int sig)
{
    if (sig == SIGINT) {
        INFO("SIGINT received, shutting down..");
        g_shouldExit = 1;
    }
}
