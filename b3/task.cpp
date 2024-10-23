#include <cassert>

#include "task.h"

using namespace b3;

void Task::stop() {
	if (!job)
		return;

	m_should_stop = true;
	assert(job->joinable());

    job->join();
    delete job;
    job = nullptr;
}

void Task::setState(State new_state) {
	std::lock_guard<std::mutex> lock(m_state_mutex);

	if (m_inner_state != new_state) {
		m_transitions.push_back({m_inner_state, new_state});
		m_inner_state = new_state;
	}
}

void Task::spawn() {
	assert(!job);

	job = new std::thread([=]() {
		m_should_stop.store(false);

		onStart();

		while (!m_should_stop.load()) {
			State current_state;
			{
				std::lock_guard<std::mutex> lock(m_state_mutex);
				current_state = m_inner_state;

				// Replay pending transitions
				for (auto& transition : m_transitions) {
					onTransition(transition.first, transition.second);
				}

				m_transitions.clear();
			}

			frame(current_state);
		}

		onStop();
	});
}
