

# bluetooth enable
bluetoothctl discoverable on
bluetoothctl pairable on

# Directory setup
mkdir -p /opt/b3/audio/
mkdir -p /tmp/b3/


flask --app app run --host=0.0.0.0 --debugger