#include "signalProcessing.h"
#include <unistd.h>

#define RUN_STANDALONE

#ifdef RUN_STANDALONE
# define signalProcessing_main main
#endif


int signalProcessing_main(void)
{
    //todo: read file name from somewhere
    char inputFile[1024];

    b3::audioProcessor processor;

    // dummy loop
    while (1) {

        usleep(processor.usToNextChunk());
    }

    return 0;
}
