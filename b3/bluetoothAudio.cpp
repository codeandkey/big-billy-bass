#include "bluetoothAudio.h"

#include "logger.h"


#define UNUSED(p) (void)p

using namespace b3;
// Callback for audio data
void read_callback(pa_stream *stream, size_t length, void *userdata)
{

    assert(userdata);
    bluetoothDefaults::pa_chunk *data = (bluetoothDefaults::pa_chunk *)userdata;
    // drop last data so we look at new data
    if (!data->first_read) {
        data->first_read = false;
        pa_stream_drop(stream);
    }

    // Peek at the data in the stream
    if (pa_stream_peek(stream, (const void **)&(data->data), &(data->bytes_available)) < 0) {
        ERROR("Failed to peek stream data");
        return;
    }

    if (data->bytes_available == 0) {
        if (!data->data) {
            DEBUG("End of stream detected!");
            return;
        }
        // this means a "hole" was detected - recursivec call?
        read_callback(stream, length, userdata);
    }
    return;
}

// Callback for stream state changes
void stream_state_callback(pa_stream *stream, void *mainloop)
{
    pa_stream_state_t state = pa_stream_get_state(stream);
    if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED) {
        ERROR("Stream error or termination");
        pa_mainloop_quit((pa_mainloop *)mainloop, 1);
    }

}

void source_info_callback(pa_context *c, const pa_source_info *info, int eol, void *userdata)
{
    UNUSED(c);
    assert(userdata);
    bluetoothDefaults::source_data *data = (bluetoothDefaults::source_data *)userdata;

    if (eol > 0) return;  // End of list
    INFO("New source detected:");
    INFO("---Source Name: %s", info->name);
    INFO("---Description: %s", info->description);
    INFO("---Sample Rate: %u Hz", info->sample_spec.rate);
    INFO("---Channels: %u", info->sample_spec.channels);
    INFO("---Format: %u", info->sample_spec.format);

    data->ch_count = info->sample_spec.rate;
    data->sample_rate = info->sample_spec.rate;
}

// Callback for context state changes
void context_state_callback(pa_context *context, void *pa_data)
{
    assert(pa_data);
    bluetoothDefaults::pa_data *data = (bluetoothDefaults::pa_data *)pa_data;
    pa_context_state_t state = pa_context_get_state(context);
    if (state == PA_CONTEXT_READY)
        DEBUG("PulseAudio context is ready.");
        pa_operation *o = pa_context_get_source_info_list(context, source_info_callback, &data->data);
        if (o) pa_operation_unref(o);
    else if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
        ERROR("Context error or termination: %d", state);
        pa_mainloop_quit(data->ml, 1);
    }
    

    
}


bluetoothAudio::bluetoothAudio()
{

}

int b3::bluetoothAudio::sample_rate()
{
    return 0;
}

int b3::bluetoothAudio::ch_count()
{
    return 0;
}

int bluetoothAudio::read_chunk(pcm_t *buffer, size_t frameCount)
{
    UNUSED(buffer);
    UNUSED(frameCount);
    return 0;
}

uint64_t b3::bluetoothAudio::stream_timestamp_uS()
{
    return 0;
}

int bluetoothAudio::pa_connect()
{
    int err = 0;
    if (m_data.ml) {
        ERROR("Pulse audio already connected");
        return -1;
    }

    // connect to pa server
    m_data.ml = pa_mainloop_new();
    m_data.api = pa_mainloop_get_api(m_data.ml);
    m_data.context = pa_context_new(m_data.api, bluetoothDefaults::PA_PROGRAM_NAME);

    INFO("Awaiting pulse audio connection...");

    pa_context_set_state_callback(m_data.context, context_state_callback, m_data.ml);
    pa_context_connect(m_data.context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    while (pa_context_get_state(m_data.context) != PA_CONTEXT_READY)
        pa_mainloop_iterate(m_data.ml, 1, NULL);

    // now try and find the audio stream
    pa_sample_spec ss = {
        .format = bluetoothDefaults::SAMPLE_FORMAT,
        .rate = signalProcessingDefaults::SAMPLE_RATE,
        .channels = 2
    };
    m_data.stream = pa_stream_new(m_data.context, "Record Stream", &ss, NULL);
    if (!m_data.stream) {
        ERROR("Failed to create PulseAudio stream");
        err = -1;
        goto pa_init_fail_abort;
    }
    pa_stream_set_state_callback(m_data.stream, stream_state_callback, m_data.ml);
    pa_stream_set_read_callback(m_data.stream, read_callback, &m_data.chunk);

    if ((err = pa_stream_connect_record(m_data.stream, nullptr, nullptr, PA_STREAM_NOFLAGS)) < 0) {
        ERROR("Failed to connect to audio stream");
        goto pa_init_fail_abort;
    }

    INFO("Pulse Audio Connected!");

    return 0;

pa_init_fail_abort:
    pa_disconnect();
    return err;
}

void bluetoothAudio::pa_disconnect()
{
    if (m_data.stream) {
        pa_stream_disconnect(m_data.stream);
        pa_stream_unref(m_data.stream);
        m_data.stream = nullptr;
    }
    if (m_data.context) {
        pa_context_disconnect(m_data.context);
        pa_context_unref(m_data.context);
        m_data.context = nullptr;
    }
    if (m_data.ml) {
        pa_mainloop_free(m_data.ml);
        m_data.ml = nullptr;
    }
}
