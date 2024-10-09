#pragma once

#include "task.h"

#include <memory>

namespace b3 {

class Runner {
public:
	~Runner() {
		stopAll();
	}

	template<typename T, typename ...Args>
	void addTask(Args... args) {
		m_tasks.push_back(std::make_unique<T>(args...));
	}

	/**
	 * Changes state of all registered tasks.
	 *
	 * @param new_state New state to set.
	 */
	void setState(State new_state) {
		for (auto& task : m_tasks) {
			task->setState(new_state);
		}
	}

	/**
	 * Starts all registered tasks in the background.
	 */
	void spawn() {
		for (auto& task : m_tasks) {
			task->spawn();
		}
	}

	/**
	 * Sends a stop to all registered tasks and waits for them to finish.
	 */
	void stopAll() {
		for (auto& task : m_tasks) {
			task->stop();
		}

		m_tasks.clear();
	}

private:
	std::vector<std::unique_ptr<Task>> m_tasks;

}; // class Runner

} // namespace b3
