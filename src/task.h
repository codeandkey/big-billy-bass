#pragma once

#include <cassert>
#include <thread>

#include "state.h"

namespace b3 {

typedef std::pair<State, State> Transition;

class Task {
public:
	Task() : m_inner_state(State::STOPPED), m_should_stop(false) {}
	virtual ~Task() = default;

	/**
	 * Called repeatedly with the current state.
	 */
	virtual void frame(State state) {}

	/**
	 * Called when the current state is changed.
	 *
	 * @param from The previous state.
	 * @param to The new state.
	 */
	virtual void onTransition(State from, State to) {}

	/**
	 * Called when the task is launched.
	 */
	virtual void onStart() {}

	/**
	 * Called when the task is terminated.
	 */
	virtual void onStop() {}

	/**
	 * Sends a stop signal to the task and waits for it to complete.
	 */
	void stop();

	/**
	 * Changes the task state.
	 */
	void setState(State new_state);

	/**
	 * Spawns the task thread. Should not be called directly.
	 */
	void spawn();

private:
	// State synchronization
	std::mutex              m_state_mutex;
	State                   m_inner_state;
	std::vector<Transition> m_transitions;

	// Job control
	std::thread*     job;
	std::atomic_bool m_should_stop;
}; // class Task

} // namespace b3
