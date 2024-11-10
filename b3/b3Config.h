#pragma once


extern "C" {
#include <string.h>
}

#include <cstdio>

#include "signalProcessingDefaults.h"

namespace b3 {

    namespace configDefaults {
        constexpr const char *DEFAULT_CONFIG_PATH = "/home/billy/.config/b3.ini";
        constexpr float DEFAULT_BODY_THRESHOLD = 10000;
        constexpr float DEFAULT_MOUTH_THRESHOLD = 10000;
        constexpr float DEFAULT_RMS_WINDOW_MS = 250;
        constexpr float DEFAULT_FLIP_INTERVAL_MS = 2000;

        constexpr int DEFAULT_CONFIG_FILE_NAME_SIZE = 255;
    };


    class b3Config {
    public:
        b3Config() :
            LPF_CUTOFF(signalProcessingDefaults::HPF_CUTOFF_DEFAULT),
            HPF_CUTOFF(signalProcessingDefaults::LPF_CUTOFF_DEFAULT),
            CHUNK_SIZE_MS(signalProcessingDefaults::CHUNK_SIZE_MS),
            BUFFER_LENGTH_MS(signalProcessingDefaults::BUFFER_LENGTH_MS),
            BODY_THRESHOLD(configDefaults::DEFAULT_BODY_THRESHOLD),
            MOUTH_THRESHOLD(configDefaults::DEFAULT_MOUTH_THRESHOLD),
            CHUNK_COUNT(signalProcessingDefaults::CHUNK_COUNT),
            RMS_WINDOW_MS(configDefaults::DEFAULT_RMS_WINDOW_MS),
            FLIP_INTERVAL_MS(configDefaults::DEFAULT_FLIP_INTERVAL_MS),
            SEEK_TIME(0),
            m_configFileOpen(false)
        {
            init();
        }
        ~b3Config();

        void poll();
        void printSettings();

        float LPF_CUTOFF;
        float HPF_CUTOFF;
        float CHUNK_SIZE_MS;
        int BUFFER_LENGTH_MS;
        int BODY_THRESHOLD;
        int MOUTH_THRESHOLD;
        int CHUNK_COUNT;
        int RMS_WINDOW_MS;
        int FLIP_INTERVAL_MS;
        uint64_t SEEK_TIME;

    private:

        int init();

#define __printer(method, type, typeStr)                                                    \
        inline void method(const char *var, type value){                                    \
            if (m_configFileOpen)   fprintf(m_configFile, "%s=" typeStr "\n", var, value);  \
        }

        __printer(printVar, int, "%d")
        __printer(printVar, float, "%f")
        __printer(printVar, uint64_t, "%lu");

        inline void setComment(const char *comment)
        {
            if (m_configFileOpen)   fprintf(m_configFile, "# %s\n", comment);
        }


        bool m_configFileOpen;
        FILE *m_configFile;
    };



};
