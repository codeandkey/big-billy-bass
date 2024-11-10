from typing import Any
import configobj

"""
This file contains the configuration for the server. This includes the routes for the actions and the config, as well as the allowable actions and config keys.
"""

ACTION_ROUTE = "api/actions"
# config for the actions
PLAY = "play_pause"
STOP = "stop"
PAUSE = "pause"

# allowable actions. Pause is not needed here because the pause/play action is state dependent
ALLOWABLE_ACTIONS = [PLAY, STOP]

ACTION = "action"
ARGS = "args"


class apiAction:

    def __init__(self, action: str, args: tuple):
        if action not in ALLOWABLE_ACTIONS:
            raise ValueError(f"Invalid action: {action}")
        self.action = action
        self.args = args

    def __str__(self):
        return f"{ACTION}: {self.action}, {ARGS}: {self.args}"

    def __repr__(self):
        return f"{ACTION}: {self.action}, {ARGS}: {self.args}"

    def to_dict(self):
        return {ACTION: self.action, ARGS: self.args}

    def __getitem__(self, key):
        if key == ACTION:
            return self.action
        elif key == ARGS:
            return self.args
        else:
            raise KeyError(f"Invalid key: {key}")

    def __setitem__(self, key, value):
        if key == ACTION:
            self.action = value
        elif key == ARGS:
            self.args = value
        else:
            raise KeyError(f"Invalid key: {key}")

    @staticmethod
    def from_dict(data):
        if ACTION not in data:
            raise ValueError("No action provided")
        if ARGS not in data:
            raise ValueError("No args provided")
        return apiAction(data[ACTION], data[ARGS])


# define config file keys. This needs to match the b3Config.cpp file
CONFIG_ROUTE = "api/config"

LPF = "lpf_cutoff"
HPF = "hpf_cutoff"
BODY_THRESHOLD = "body_threshold"
MOUTH_THRESHOLD = "mouth_threshold"
CHUNK_SIZE_MS = "chunk_size_ms"
BUFFER_COUNT = "buffer_count"
SEEK_TIME = "seek_time"

ALLOWED_CONFIG_KEYS = [
    LPF,
    HPF,
    BODY_THRESHOLD,
    MOUTH_THRESHOLD,
    CHUNK_SIZE_MS,
    BUFFER_COUNT,
    SEEK_TIME,
]


class configVariable:

    def __init__(self, key, value):
        if key not in ALLOWED_CONFIG_KEYS:
            raise ValueError(f"Invalid config key: {key}")
        self.__key = key
        self.__value = value

    def __str__(self):
        return f"{self.key}: {self.value}"

    def __repr__(self):
        return f"{self.key}: {self.value}"

    def to_dict(self):
        return {self.key: self.value}

    @property
    def key(self):
        return self.__key

    @property
    def value(self):
        return self.__value


# server settings
SERVER_PORT = 5000
SERVER_HOST = "tbd"


class config:
    def __init__(self, file_path=None):
        for key in ALLOWED_CONFIG_KEYS:
            setattr(self, key, configVariable(key, None))

        if file_path is not None:
            self.read_config_file(file_path)

    def __str__(self):
        return f"{self.config}"

    def __repr__(self):
        return f"{self.config}"

    def to_dict(self):
        return self.config

    @property
    def config(self):
        return self.__config

    @staticmethod
    def from_dict(data):
        return config(data)

    def read_config_file(self, file_path):
        config = configobj.ConfigObj(file_path)

        for key in ALLOWED_CONFIG_KEYS:
            setattr(self, key, config[key])
            

        pass

    @staticmethod
    def update_config_file(self, file_path):
        config = configobj.ConfigObj(file_path)
        for key in ALLOWED_CONFIG_KEYS:
            config[key] = getattr(self, key)
        config.write()

    def __getitem__(self, key):
        if key in ALLOWED_CONFIG_KEYS:
            return getattr(self, key)
        else:
            raise KeyError(f"Invalid key: {key}")
    
    def __setitem__(self, key, value):
        if key in ALLOWED_CONFIG_KEYS:
            setattr(self, key, value)
        else:
            raise KeyError(f"Invalid key: {key}")
        