#!/bin/bash

# ensure pulse is going. If our program runs as root, this program also needs to run as root.
pulseaudio --start 
# ensure bt-agent is going
systemctl start bt-agent

# bluetooth enable
bluetoothctl discoverable on
bluetoothctl pairable on

flask --app app run --host=0.0.0.0 --debugger