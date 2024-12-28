#!/bin/bash
install_if_not_present() {
    if ! dpkg -l | grep -q "$1"; then
        apt-get install -y "$1"
    else
        echo "$1 is already installed"
    fi
}



install_pigpio_if_not_present() {
    if ! command -v pigpiod &> /dev/null; then
        wget https://github.com/joan2937/pigpio/archive/master.zip
        unzip master.zip
        cd pigpio-master
        make
        make install
        cd ..
        rm -rf pigpio-master master.zip
    else
        echo "pigpio is already installed"
    fi
}


# Install cmake if not present
install_if_not_present cmake

# Install C++ dependencies if not present
install_if_not_present libavcodec-dev
install_if_not_present libavformat-dev
install_if_not_present libasound2-dev

# Install pip if not present
install_if_not_present python3-pip

# Install required python libs if not present
install_if_not_present python3-flask
install_if_not_present python3-configobj

# install pigpio if not present
install_pigpio_if_not_present