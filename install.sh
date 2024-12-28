#!/bin/bash

# Directory setup
mkdir -pv /opt/b3/ 
mkdir -pv /tmp/b3/
cp -rv audio /opt/b3/

# build program
cmake -B build
make -C build -j4

# install
cp -rv build/b3/b3 /usr/local/bin/b3