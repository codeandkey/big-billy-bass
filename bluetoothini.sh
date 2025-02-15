#!/bin/bash

# config file for setting up a bluetooth sink

# install pulse audio
apt-get install -y pulseaudio pulseaudio-module-bluetooth

# add user to bluetooth group
echo "...Adding $(whoami) user to bluetooth group"
usermod -a -G bluetooth $(whoami)


echo "...updating bluetooth main.conf"
# update bluetooth config
sed -Ei 's/^\s*#?\s*Class = 0x[0-9A-Fa-f]+/Class = 0x41C/; s/^\s*#?\s*DiscoverableTimeout = [0-9]+/DiscoverableTimeout = 0/' /etc/bluetooth/main.conf



#restart bluetooth
echo "...re-starting bluetooth"
bluetoothctl power off
systemctl stop bt-agent
systemctl restart bluetooth



# enable pulse audio on boot
echo "...enabeling pulseaudio on boot"
systemctl --user enable pulseaudio



# auto-pairing steup
apt-get install -y bluez-tools

# create bluetooth agent 
echo "[Unit]
Description=Bluetooth Auth Agent
After=bluetooth.service
PartOf=bluetooth.service

[Service]
Type=simple
ExecStart=/usr/bin/bt-agent -c NoInputNoOutput

[Install]
WantedBy=bluetooth.target" > /etc/systemd/system/bt-agent.service



#start pulseaudio
echo "...starting pulseaudio"
pulseaudio --start

# create agent
echo "...creating bluetooth agent pulseaudio"
sudo systemctl enable bt-agent
sudo systemctl start bt-agent
# status
systemctl status bt-agent


echo "Full install requries a restart!"