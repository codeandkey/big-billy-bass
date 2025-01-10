#include "b3Config.h"

#include <unordered_map>
#include <functional>
#include <string>

#include "signalProcessingDefaults.h"
#include "programPaths.h"
#include "logger.h"

using namespace b3;
namespace SPD = signalProcessingDefaults;


namespace configVars {
    constexpr const char *LPF = "lpf_cutoff";
    constexpr const char *HPF = "hpf_cutoff";
    constexpr const char *BODY_THRESHOLD = "body_threshold";
    constexpr const char *MOUTH_THRESHOLD = "mouth_threshold";
    constexpr const char *RMS_WINDOW_MS = "rms_window_ms";
    constexpr const char *CHUNK_SIZE_MS = "chunk_size_ms";
    constexpr const char *FLIP_INTERVAL_MS = "flip_interval_ms";
    constexpr const char *BUFFER_COUNT = "buffer_count";
    constexpr const char *SEEK_TIME = "seek_time";



    std::function<void(int &, std::string)> assignInt = [](int &i, std::string value) {i = std::stoi(value);};
    std::function<void(float &, std::string)> assignFloat = [](float &f, std::string value) {f = std::stof(value);};
    std::function<void(uint64_t &, std::string)> assignU64 = [](uint64_t &i, std::string value) {i = std::stoull(value);};

    std::unordered_map<std::string, std::function<void(b3Config &, std::string)>> g_configMap = {
        {LPF,               [](b3Config &cfg, std::string value) {assignFloat(cfg.LPF_CUTOFF, value);}},
        {HPF,               [](b3Config &cfg, std::string value) {assignFloat(cfg.HPF_CUTOFF, value);}},
        {CHUNK_SIZE_MS,     [](b3Config &cfg, std::string value) {assignFloat(cfg.CHUNK_SIZE_MS, value);}},
        {BODY_THRESHOLD,    [](b3Config &cfg, std::string value) {assignInt(cfg.BODY_THRESHOLD, value);}},
        {MOUTH_THRESHOLD,   [](b3Config &cfg, std::string value) {assignInt(cfg.MOUTH_THRESHOLD, value);}},
        {RMS_WINDOW_MS,     [](b3Config &cfg, std::string value) {assignInt(cfg.RMS_WINDOW_MS, value);}},
        {FLIP_INTERVAL_MS,  [](b3Config &cfg, std::string value) {assignInt(cfg.FLIP_INTERVAL_MS, value);}},
        {BUFFER_COUNT,      [](b3Config &cfg, std::string value) {assignInt(cfg.CHUNK_COUNT, value);}},
        {SEEK_TIME,         [](b3Config &cfg, std::string value) {assignU64(cfg.SEEK_TIME, value);}}
    };
};

namespace commandlineArgs {
    constexpr const char *VERBOSE = "-v";
    constexpr const char *AUDIO_FILE = "-f";
    constexpr const char *LPF_CUTOFF = "-lpf";
    constexpr const char *HPF_CUTOFF = "-hpf";
    constexpr const char *SEEK_TIME = "-seek";
    constexpr const char *BODY_THRESHOLD = "-body";
    constexpr const char *MOUTH_THRESHOLD = "-mouth";

    std::unordered_map<std::string, std::function<void(b3Config &)>> g_flags = {
        {VERBOSE,[](b3Config &cfg) {
            SET_VERBOSE_LOGGING(true);
            INFO("Enabled Verbose Logging");
            (void)cfg;  // shut up compiler
        }}
    };

    std::unordered_map<std::string, std::function<void(b3Config &, std::string)>> g_keyValue = {
        {LPF_CUTOFF,[](b3Config &cfg, std::string value) {
            cfg.LPF_CUTOFF = stof(value);
            INFO("Configured LPF Cutoff to %f",cfg.LPF_CUTOFF);
        }},

        {HPF_CUTOFF,[](b3Config &cfg, std::string value) {
            cfg.HPF_CUTOFF = stof(value);
            INFO("Configured HPF Cutoff to %f", cfg.HPF_CUTOFF);
        }},

        {BODY_THRESHOLD,[](b3Config &cfg, std::string value) {
            cfg.BODY_THRESHOLD = stoi(value);
            INFO("Configured Body Threshold to %d", cfg.BODY_THRESHOLD);
        }},

        {MOUTH_THRESHOLD,[](b3Config &cfg, std::string value) {
            cfg.MOUTH_THRESHOLD = stoi(value);
            INFO("Configured Mouth Threshold to %d", cfg.MOUTH_THRESHOLD);
        }},

        {SEEK_TIME,[](b3Config &cfg, std::string value) {
            cfg.SEEK_TIME = stoull(value);
            INFO("Configured Seek time to %llu", cfg.SEEK_TIME);
        }},

        {AUDIO_FILE,[](b3Config &cfg, std::string value) {
            cfg.PROGRAM_OP_MODE = file_based;
                snprintf(
                    cfg.ACTIVE_FILE,
                    configDefaults::FILE_NAME_BUFFER_SIZE,
                    "%s/%s",
                    programPaths::AUDIO_PATH,
                    value.c_str());
            INFO("Configured file to load: %s", cfg.ACTIVE_FILE);
        }}
    };
};


void trim_white_space(char *str)
{
    // Trim leading whitespace
    int i = 0;
    while (isspace(str[i])) {
        i++;
    }
    memmove(str, str + i, strlen(str) - i + 1);

    // Trim trailing whitespace
    i = strlen(str) - 1;
    while (i >= 0 && isspace(str[i])) {
        str[i] = '\0';
        i--;
    }
}

b3::b3Config::~b3Config()
{
    if (m_config_file)
        fclose(m_config_file);
    m_config_file = nullptr;
}

void b3::b3Config::parse_cmd_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (commandlineArgs::g_flags.count(argv[i]) > 0)
            commandlineArgs::g_flags[argv[i]](*this);
        else if (i + 1 < argc && commandlineArgs::g_keyValue.count(argv[i]) > 0) {
            commandlineArgs::g_keyValue[argv[i]](*this, argv[i + 1]);
            i++;
        } else
            WARNING("Ignoring unrecognized commandline argument: %s", argv[i]);
    }
}

void b3::b3Config::poll()
{
    if (!m_config_file)
        return;

    char line[255];

    while (fgets(line, sizeof(line), m_config_file)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (!value)
            continue;
        trim_white_space(key);
        trim_white_space(line);

        if (configVars::g_configMap.count(key) > 0)
            configVars::g_configMap[key](*this, value);
    }

    BUFFER_LENGTH_MS = CHUNK_COUNT * CHUNK_SIZE_MS;

    fclose(m_config_file);
    m_config_file = nullptr;
}

void b3::b3Config::print_settings()
{
    // make sure config file is opened w/ write access
    bool keep_open = m_config_file;
    if (m_config_file)
        fclose(m_config_file);

    m_config_file = fopen(m_config_file_name, "w");

    if (!m_config_file) {
        ERROR("Error opening config file %s with write access", m_config_file_name);
        return;
    }

    set_comment("The following parameters may be written too and the program will update");
    print_var(configVars::HPF, HPF_CUTOFF);
    print_var(configVars::LPF, LPF_CUTOFF);
    print_var(configVars::BODY_THRESHOLD, BODY_THRESHOLD);
    print_var(configVars::MOUTH_THRESHOLD, MOUTH_THRESHOLD);
    print_var(configVars::BUFFER_COUNT, CHUNK_COUNT);
    print_var(configVars::RMS_WINDOW_MS, RMS_WINDOW_MS);
    print_var(configVars::FLIP_INTERVAL_MS, FLIP_INTERVAL_MS);
    set_comment("The following parameters are loaded at the beginning of the program and do not update");
    print_var(configVars::CHUNK_SIZE_MS, CHUNK_SIZE_MS);
    print_var(configVars::SEEK_TIME, SEEK_TIME);

    // open file back up in read only
    fclose(m_config_file);

    if (keep_open)
        m_config_file = fopen(m_config_file_name, "r");
    else
        m_config_file = nullptr;

}


int b3::b3Config::init()
{
    m_config_file = fopen(m_config_file_name, "r");
    if (!m_config_file)
        WARNING("Unable to find %s, using default values", m_config_file_name);
    else
        poll();

    print_settings();

    return 0;
}




