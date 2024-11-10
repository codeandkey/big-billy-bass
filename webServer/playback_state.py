import os
import subprocess
import signal
import time
import glob
from app_config import *


class playBackConfig:
    DEFAULT_CONFIG_FILE_NAME = "b3.ini"
    CONFIG_PATH_FROM_ROOT = "/home/billy/.config"
    CONFIG_FILE = os.path.join(CONFIG_PATH_FROM_ROOT, DEFAULT_CONFIG_FILE_NAME)
    AUDIO_FILE_PATH = os.path.join("/","opt","b3","audio")
    EXEC_FILE = os.path.join("/", "home","billy","big-billy-bass","build", "b3", "b3")


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
            self.state = PAUSE
            print("pausing")
            
        elif self.state == STOP or self.state == PAUSE:
            # transition from pause or stop to play.
            # If paused, the seek time is already set by the return code of the process
            args = a[ARGS]
            self.activeFile = args

            # stop the current process if it exists
            if self.process:
                self.stop_process()

            # start the new process
            self.start_process(a[ARGS])
            self.state = PLAY
            print("playing")

    def to_stop(self, a: apiAction):
        if self.state == PLAY:
            self.stop_process()
            self.state = STOP
        elif self.state == PAUSE:
            self.conf[SEEK_TIME] = 0
        elif self.state == STOP:
            pass
        self.activeFile = "";
        self.state = STOP 
        self.conf.update_config_file(playBackConfig.CONFIG_FILE)
        print("stopping")
        

    def start_process(self, file):
        print(f"File Name to load: {file} from {self.state}");
        seek = str(self.conf[SEEK_TIME]) if self.state == PAUSE else "0";
        self.process = subprocess.Popen(
            [playBackConfig.EXEC_FILE, "-f", file, "-seek", seek, "-v"]
        )
    

    def stop_process(self):
        self.process.send_signal(signal.SIGINT)
        
            
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        # if self.process.returncode != 0:
        #     raise ValueError(
        #         f"Process terminated with signal {self.process.returncode}"
        #     )
        self.process = None
        # update config file
        self.conf.read_config_file(playBackConfig.CONFIG_FILE)

    def update_config_file(self, jsonData):
        self.conf.from_json(jsonData);
        self.conf.update_config_file(playBackConfig.CONFIG_FILE)
        
    def read_config_file(self):
        self.conf.read_config_file(playBackConfig.CONFIG_FILE)
        return self.conf.to_dict()
        
    def kill(self):
        try:
            self.process.kill()
        except:
            pass
        
    def get_files(self):
        l = list(map(os.path.basename,glob.iglob(os.path.join(playBackConfig.AUDIO_FILE_PATH,"*.mp3"))));
        l.sort(key=lambda a: a.lower())
        return l

    def active_file(self):
        return self.activeFile

    def check_state(self):
        
        if self.process is not None and self.process.poll() is not None:
            a = apiAction(STOP, "")
            self.perform_action(a);


    def download_yt(self,url):
        print(f"Downloading yt from: {url}")
        self.dl_proc = subprocess.Popen(
            [
                f"yt-dlp {url}"
                "-f bestaudio" 
                "--extract-audio"
                "--audio-quality 0" 
                "--audio-format mp3"
                "-o /opt/b3/audio/tmp.mp3"
            ]
        );
        
        
