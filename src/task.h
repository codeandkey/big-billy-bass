#pragma once

#include "state.h"

namespace b3 {

class Task {
public:
	/**
	 * Called repeatedly while the current state is PLAYING.
	 */
	virtual void frame();

	/**
	 * Called when the current state is changed.
	 *
	 * @param from The previous state.
	 * @param to The new state.
	 */
	virtual void onStateChange(State from, State to);

	/**
	 * Get the current task state.
	 */
	const State& getState() {
		return m_inner_state;
	}

protected:
	void _onStateChange(State from, State to) {
		m_inner_state = to;
		onStateChange(from, to);
	}

private:
	State m_inner_state;
}; // class Task

} // namespace b3
