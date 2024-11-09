
#pragma once


namespace signalHandler {

    extern volatile int g_shouldExit;

    void sigintHandler(int sig);

};