#pragma once

#include <string>

namespace b3 {

enum State {
	STOPPED, // No audio file is loaded or playing
	PLAYING, // The audio file is currently playing
	PAUSED   // There is an audio file loaded, but not playing
};

struct Options {
	std::string filename;
};

} // namespace b3
