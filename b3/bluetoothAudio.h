#pragma once

#include "audioSource.h"

extern "C" {
#include <pulse/pulseaudio.h>
}

#include "signalProcessingDefaults.h"

namespace b3 {
    namespace bluetoothDefaults {
        constexpr pa_sample_format_t SAMPLE_FORMAT = signalProcessingDefaults::__default_fmt_selector(
            PA_SAMPLE_S16NE,
            PA_SAMPLE_S32NE,
            PA_SAMPLE_FLOAT32NE,
            PA_SAMPLE_INVALID
        );
        constexpr const char *PA_PROGRAM_NAME = "Big-Billy-Bass";

        struct pa_chunk {
            pa_chunk() :data(nullptr), bytes_available(0), first_read(true) {}
            void *data;
            size_t bytes_available;
            bool first_read;
        };

        struct source_data {
            source_data() : sample_rate(0), ch_count(0), timestamp_us(0) {}
            int sample_rate;
            int ch_count;
            uint64_t timestamp_us;
        };


        struct pa_data {
            pa_data() : ml(nullptr), api(nullptr), context(nullptr), stream(nullptr) {}
            pa_mainloop *ml;
            pa_mainloop_api *api;
            pa_context *context;
            pa_stream *stream;

            pa_chunk chunk;
            source_data data;
        };

    };

    // wrapper class for simple pulse audio interface
    class bluetoothAudio : public audioSource {
    public:

        bluetoothAudio();
        inline ~bluetoothAudio() { pa_disconnect(); }

                // sample rate (frames/second) of the current active stream
        int sample_rate() override;

        // \# of audio channels in active stream
        int ch_count() override;

        //  blocking read of framecount number of frames into pcm buffer (frameCount * channel total samples)
        //  Read of 0 means no data available
        int read_chunk(pcm_t *buffer, size_t frameCount) override;

        // timestamp in stream for current loaded media
        uint64_t stream_timestamp_uS() override;

        // pa specific functions
        int pa_connect();

        void pa_disconnect();

    private:
        bluetoothDefaults::pa_data m_data;
    };
};

