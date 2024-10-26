#include <cstdio>
#include <chrono>
#include <cstring>
#include <string>

extern "C" {
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
}

#include "gpioStream.h"
#include "sigint.h"
#include "logger.h"
#include "runner.h"
#include "signalProcessing.h"
#include "state.h"
#include "task.h"
#include "audioDriver.h"
#include "audioFile.h"

constexpr const char* SOCKPATH = "/tmp/b3.sock";

using namespace b3;

static std::atomic_bool sigint_flag;

void sigint_handler(int _sig) {
    INFO("In SIGINT handler");
    sigint_flag.store(true);
}

class EchoTask : public Task {
    public:
        ~EchoTask() = default;

        void onStart() override { INFO("Started test task"); }
        void onStop() override { INFO("Stopped test task"); }

        void frame(State state) override {
            std::this_thread::sleep_for(std::chrono::duration<double>(0.25));
        }

        void onTransition(State from, State to) override {
            static const char* states[] {
                "STOPPED",
                "PLAYING",
                "PAUSED"
            };
            INFO("State changed from %s to %s", states[from], states[to]);
        }
};

int main(int argc, char** argv) {
    Runner runner;
    float lpf = signalProcessingDefaults::LPF_CUTOFF_DEFAULT;
    float hpf = signalProcessingDefaults::HPF_CUTOFF_DEFAULT;
    char fileName[255] = "test.mp3";
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "-v" || std::string(argv[i]) == "--verbose") {
            SET_VERBOSE_LOGGING(true);
            INFO("Verbose logging enabled");
        }
        if (std::string(argv[i]) == "-f" && i + 1 < argc) {
            strncpy(fileName, argv[i + 1], sizeof(fileName));
            INFO("loading sound file: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-lpf" && i + 1 < argc) {
            lpf = std::stod(argv[i+1]);
            INFO("LPF setting: %s", argv[i + 1]);
            i++;
        }
        if (std::string(argv[i]) == "-hpf" && i + 1 < argc) {
            lpf = std::stod(argv[i + 1]);
            INFO("HPF setting: %s", argv[i + 1]);
            i++;
        }
    }

    audioDriver driver = audioDriver();
    audioFile file = audioFile(fileName);

    runner.addTask<EchoTask>();

#ifndef GPIO_TEST
    signalProcessor *processor = runner.addTask<signalProcessor>();
    processor->setAudioDriver(&driver);
    processor->setLPF(lpf);
    processor->setHPF(hpf);
#else
#warning GPIO_TEST defined, not loading audio processor
    WARNING("GPIO_TEST defined, not loading audio processor");
#endif // GPIO_TEST

    runner.addTask<GpioStream>();

    INFO("Spawning tasks");
    runner.spawn();

    INFO("Starting play");
                                
    processor->setFile(&file);

    runner.setState(State::PLAYING);

    auto handleconn = [&](int sock) {
        char buf[1024];
        ssize_t n;
        while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
            buf[n] = '\0';

            // Strip whitespace
            for (char* i = &buf[n - 1]; i >= buf && isspace(*i); --i) {
                *i = '\0';
            }

            INFO("Received: %s", buf);

            if (std::string(buf) == "stop") {
                runner.setState(State::STOPPED);
            } else if (std::string(buf) == "play") {
                runner.setState(State::PLAYING);
            } else if (std::string(buf) == "pause") {
                runner.setState(State::PAUSED);
            } else if (std::string(buf) == "quit") {
                break;
            }

            if (!strncmp(buf, "load", 4)) {
                if (n < 6) {
                    ERROR("Invalid load command");
                    continue;
                }

                std::string path = std::string(buf + 5);
                INFO("Loading file: %s", path.c_str());
            }
        }
        close(sock);
        INFO("Terminated session");
    };

    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        ERROR("Failed to create socket");
        return 1;
    }

    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKPATH, sizeof(addr.sun_path) - 1);

    if (unlink(SOCKPATH) < 0) {
        perror("unlink");
        ERROR("Failed to unlink socket");
    }

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        ERROR("Failed to bind socket");
        return 1;
    }

    if (listen(listener, SOMAXCONN) < 0) {
        perror("listen");
        ERROR("Failed to listen on socket");
        return 1;
    }

    // Activate SIGINT watchdog
    std::thread sigint_watchdog = std::thread([&]() {
        while (1) {
            std::this_thread::sleep_for(std::chrono::duration<double>(0.25));
            if (sigint_flag) {
                INFO("SIGINT received, terminating");
                runner.stopAll();
                close(listener);
                unlink(SOCKPATH);
                exit(0);
            }
        }
    });

    INFO("Waiting for connections..");
    while (1) {
        int sock = accept(listener, nullptr, nullptr);
        if (sock < 0) {
            perror("accept");
            ERROR("Failed to accept connection");
            break;
        }

        INFO("Accepted connection");

        // Some threads may die, but that's a sacrifice I'm willing to make
        std::thread([&]() { handleconn(sock); }).detach();
    }

    // Actually unreachable. TODO signal() for SIGINT
    runner.stopAll();
    close(listener);
    return 0;
}
