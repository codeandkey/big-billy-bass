import os
import subprocess
import signal
from app_config import *


class playBackConfig:
    DEFAULT_CONFIG_FILE_NAME = "b3.ini"
    CONFIG_PATH_FROM_ROOT = "config"
    ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
    CONFIG_FILE = os.path.join(ROOT, CONFIG_PATH_FROM_ROOT, DEFAULT_CONFIG_FILE_NAME)
    EXEC_FILE = os.path.join(ROOT, "build", "bin", "b3")


class PlaybackState:
    """
    The PlaybackState class is responsible for managing the state of the playback process.
    It can transition between the states of PLAY, PAUSE, and STOP.

    """

    process: subprocess.Popen = None
    activeFile: str = None
    state: str = None
    conf: config = None

    def __init__(self):
        self.state = STOP
        self.conf = config(playBackConfig.CONFIG_FILE)

    def perform_action(self, a: apiAction):
        if a[ACTION] == PLAY:
            self.to_pause_or_play(a)
        elif a[ACTION] == STOP:
            self.to_stop(a)
        else:
            raise ValueError(f"Invalid action: {a[ACTION]}")

    def get_state(self):
        return self.state

    def to_pause_or_play(self, a: apiAction):
        if self.state == PLAY:
            # transition from play to pause
            self.stop_process()
        elif self.state == STOP or self.state == PAUSE:
            # transition from pause or stop to play.
            # If paused, the seek time is already set by the return code of the process
            args = a[ARGS]
            self.activeFile = args[0]

            # stop the current process if it exists
            if self.process:
                self.stop_process()

            # start the new process
            self.start_process(self.activeFile)
            self.state = PLAY

    def to_stop(self, a: apiAction):
        if self.state == PLAY:
            self.stop_process()
            self.state = STOP
        elif self.state == PAUSE:
            self.seekTime = 0
        elif self.state == STOP:
            pass
        self.state = STOP

    def start_process(self, file):
        try:
            self.process = subprocess.Popen(
                [playBackConfig.EXEC_FILE, "-f", file, "-seek", str(self.conf[SEEK_TIME])]
            )
        except FileNotFoundError:
            print("Error: Could not find executable file")

    def stop_process(self):
        self.process.send_signal(signal.SIGINT)
        
            
        self.process = None
        try:
            self.process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        if self.process.returncode != 0:
            raise ValueError(
                f"Process terminated with signal {self.process.returncode}"
            )
        # update config file
        self.conf.read_config_file(playBackConfig.CONFIG_FILE)

    def update_config_file(self, file, option: configVariable):
        self.conf[option.key()] = option.value()
        self.conf.update_config_file(file)
        
    def kill(self):
        try:
            self.process.kill()
        except:
            pass
            


# quick little test
if __name__ == "__main__":
    # Initialize PlaybackState
    playback_state = PlaybackState()

    # Create a play action
    play_action = apiAction(PLAY, ("test_file.mp3",))

    # Perform play action
    playback_state.perform_action(play_action)
    print(f"State after play action: {playback_state.get_state()}")

    # Create a stop action
    stop_action = apiAction(STOP, ())

    # Perform stop action
    playback_state.perform_action(stop_action)
    print(f"State after stop action: {playback_state.get_state()}")