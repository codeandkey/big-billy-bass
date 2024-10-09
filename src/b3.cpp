#include <iostream>

#include "runner.h"
#include "task.h"
#include "state.h"

using namespace b3;

class EchoTask : public Task {
	public:
		~EchoTask() = default;

		void onStart() override { std::cout << "Started test task" << std::endl; }
		void onStop() override { std::cout << "Stopped test task" << std::endl; }

		void frame(State state) override {
			std::cout << "Frame " << state << std::endl;
			std::this_thread::sleep_for(std::chrono::duration<double>(0.25));
		}

		void onTransition(State from, State to) override {
			std::cout << "State changed from " << from << " to " << to << std::endl;
		}
};

int main(int argc, char** argv) {
	Runner runner;

	runner.addTask<EchoTask>();
	runner.spawn();

	while (1) {
		std::this_thread::sleep_for(std::chrono::duration<double>(1));
		runner.setState((State) (rand() % 3));
	}

	return 0;
}
