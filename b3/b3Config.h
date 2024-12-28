#pragma once


extern "C" {
#include <string.h>
}

#include <cstdio>

#include "signalProcessingDefaults.h"
#include "programPaths.h"

namespace b3 {
    enum op_mode {
        bluetooth,
        file_based
    };

    namespace configDefaults {
        constexpr const char *DEFAULT_CONFIG_FNAME = "b3.ini";
        constexpr size_t FILE_NAME_BUFFER_SIZE = 255;
        constexpr float DEFAULT_BODY_THRESHOLD = 10000;
        constexpr float DEFAULT_MOUTH_THRESHOLD = 10000;
        constexpr float DEFAULT_RMS_WINDOW_MS = 250;
        constexpr float DEFAULT_FLIP_INTERVAL_MS = 2000;
        constexpr op_mode DEFAULT_OP_MODE = bluetooth;      // assume bluetooth unless the -f flag provides a file       
    };


    class b3Config {
    public:
        b3Config() :
            LPF_CUTOFF(signalProcessingDefaults::HPF_CUTOFF),
            HPF_CUTOFF(signalProcessingDefaults::LPF_CUTOFF),
            CHUNK_SIZE_MS(signalProcessingDefaults::CHUNK_SIZE_MS),
            BUFFER_LENGTH_MS(signalProcessingDefaults::BUFFER_LENGTH_MS),
            BODY_THRESHOLD(configDefaults::DEFAULT_BODY_THRESHOLD),
            MOUTH_THRESHOLD(configDefaults::DEFAULT_MOUTH_THRESHOLD),
            CHUNK_COUNT(signalProcessingDefaults::CHUNK_COUNT),
            RMS_WINDOW_MS(configDefaults::DEFAULT_RMS_WINDOW_MS),
            FLIP_INTERVAL_MS(configDefaults::DEFAULT_FLIP_INTERVAL_MS),
            PROGRAM_OP_MODE(configDefaults::DEFAULT_OP_MODE),
            SEEK_TIME(0),
            m_config_file(nullptr)
        {
            snprintf(
                m_config_file_name,
                sizeof(m_config_file_name),
                "%s/%s",
                programPaths::CONFIG_PATH,
                configDefaults::DEFAULT_CONFIG_FNAME);;
            init();
        }
        ~b3Config();
        void parse_cmd_args(int argc, char **argv);
        void poll();
        void print_settings();

        float LPF_CUTOFF;
        float HPF_CUTOFF;
        float CHUNK_SIZE_MS;
        int BUFFER_LENGTH_MS;
        int BODY_THRESHOLD;
        int MOUTH_THRESHOLD;
        int CHUNK_COUNT;
        int RMS_WINDOW_MS;
        int FLIP_INTERVAL_MS;
        char ACTIVE_FILE[configDefaults::FILE_NAME_BUFFER_SIZE];
        op_mode PROGRAM_OP_MODE;
        uint64_t SEEK_TIME;

    private:
        char m_config_file_name[configDefaults::FILE_NAME_BUFFER_SIZE];
        int init();

#define __printer(method, type, typeStr)                                                    \
        inline void method(const char *var, type value){                                    \
            if (m_config_file)   fprintf(m_config_file, "%s=" typeStr "\n", var, value);  \
        }

        __printer(print_var, int, "%d")
            __printer(print_var, float, "%f")
            __printer(print_var, uint64_t, "%lu");

        inline void set_comment(const char *comment)
        {
            if (m_config_file)   fprintf(m_config_file, "# %s\n", comment);
        }

        FILE *m_config_file;
    };
};
