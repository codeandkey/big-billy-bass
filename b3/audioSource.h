#pragma once

#include "signalProcessingDefaults.h"
#include "b3Config.h"

namespace b3 {

    class audioSource {
    public:
        // sample rate (frames/second) of the current active stream
        virtual int sample_rate() = 0;

        // \# of audio channels in active stream
        virtual int ch_count() = 0;

        //  blocking read of framecount number of frames into pcm buffer (frameCount * channel total samples)
        //  Read of 0 means no data available
        virtual int read_chunk(pcm_t *buffer, size_t frameCount) = 0;

        // timestamp in stream for current loaded media
        virtual uint64_t stream_timestamp_uS() = 0;

        // used to ask nicely if we can set this
        // virtual int set_output_sample_rate() = 0;

        // // used to ask nicely if we can set this
        // virtual int set_output_ch_count() = 0;
    };
}