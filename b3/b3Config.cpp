#include "b3Config.h"

#include <unordered_map>
#include <functional>
#include <string>

#include "signalProcessingDefaults.h"
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
    constexpr const char *BUFFER_COUNT = "buffer_count";



    std::function<void(int &, std::string)> assignInt = [](int &i, std::string value) {i = std::stoi(value);};
    std::function<void(float &, std::string)> assignFloat = [](float &f, std::string value) {f = std::stof(value);};

    std::unordered_map<std::string, std::function<void(b3Config &, std::string)>> g_configMap = {
        {LPF,               [](b3Config &cfg, std::string value) {assignFloat(cfg.LPF_CUTOFF, value);}},
        {HPF,               [](b3Config &cfg, std::string value) {assignFloat(cfg.HPF_CUTOFF, value);}},
        {CHUNK_SIZE_MS,     [](b3Config &cfg, std::string value) {assignFloat(cfg.CHUNK_SIZE_MS, value);}},
        {BODY_THRESHOLD,    [](b3Config &cfg, std::string value) {assignInt(cfg.BODY_THRESHOLD, value);}},
        {MOUTH_THRESHOLD,   [](b3Config &cfg, std::string value) {assignInt(cfg.BODY_THRESHOLD, value);}},
        {RMS_WINDOW_MS,     [](b3Config &cfg, std::string value) {assignInt(cfg.RMS_WINDOW_MS, value);}},
        {BUFFER_COUNT,      [](b3Config &cfg, std::string value) {assignInt(cfg.CHUNK_COUNT, value);}}
    };
};


void trimWhiteSpace(char *str)
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
    if (m_configFileOpen)
        fclose(m_configFile);
}

void b3::b3Config::poll()
{
    if (!m_configFileOpen)
        m_configFile = fopen(configDefaults::DEFAULT_CONFIG_PATH, "r");
    if (!m_configFile)
        return;


    char line[255];

    while (fgets(line, sizeof(line), m_configFile)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (!value)
            continue;
        trimWhiteSpace(key);
        trimWhiteSpace(line);

        if (configVars::g_configMap.count(key) > 0)
            configVars::g_configMap[key](*this, value);
    }

    BUFFER_LENGTH_MS = CHUNK_COUNT * CHUNK_SIZE_MS;

    fclose(m_configFile);
    m_configFileOpen = false;
}

void b3::b3Config::printSettings()
{
    // make sure config file is opened w/ write access
    bool tmpConfigOpen = m_configFileOpen;
    if (tmpConfigOpen)
        fclose(m_configFile);
    
    m_configFile = fopen(configDefaults::DEFAULT_CONFIG_PATH, "w");

    if (!m_configFile) {
        ERROR("Error opening config file %s with write access", configDefaults::DEFAULT_CONFIG_PATH);
        return;
    }
    m_configFileOpen = true;
    setComment("The following parameters may be written too and the program will update");
    printVar(configVars::HPF, HPF_CUTOFF);
    printVar(configVars::LPF, LPF_CUTOFF);
    printVar(configVars::BODY_THRESHOLD, BODY_THRESHOLD);
    printVar(configVars::MOUTH_THRESHOLD, MOUTH_THRESHOLD);
    printVar(configVars::BUFFER_COUNT, CHUNK_COUNT);
    setComment("The following parameters are loaded at the beginning of the program and do not update");
    printVar(configVars::CHUNK_SIZE_MS, CHUNK_SIZE_MS);
    m_configFileOpen = tmpConfigOpen;

    // open file back up in read only
    fclose(m_configFile);

    if (m_configFileOpen)
        m_configFile = fopen(configDefaults::DEFAULT_CONFIG_PATH, "r");

}


int b3::b3Config::init()
{
    m_configFile = fopen(configDefaults::DEFAULT_CONFIG_PATH, "r");
    if (!m_configFile)
        WARNING("Unable to find %s, using default values", configDefaults::DEFAULT_CONFIG_PATH);
    else
        poll();

    printSettings();

    return 0;
}




