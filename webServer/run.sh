#!/bin/bash

# bluetooth enable
bluetoothctl discoverable on
bluetoothctl pairable on

flask --app app run --host=0.0.0.0 --debugger