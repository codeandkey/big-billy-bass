#!/bin/bash

# === Configurable variables ===
gpio="bbb_gpio"
signal_processing="bbb_sp"
sp_path="/usr/local/bin/$signal_processing"
gpio_path="/usr/local/bin/$gpio"

echo "[*] Setting up Billy's GPIO and Signal Processing services..."
cargo build --release


if [ $? -ne 0 ]; then
    echo "[!] Build failed. Please check the output for errors."
    exit 1
fi
echo "[*] Build successful"

sudo systemctl stop $gpio 
systemctl --user stop $signal_processing
echo "[*] Copying binaries to
    ${gpio_path}
    ${sp_path}"
sudo mkdir -p /usr/local/bin/billy
systemctl --user stop $signal_processing
sudo systemctl stop $gpio
sudo cp ./target/release/gpio $gpio_path
sudo cp ./target/release/signal_processing $sp_path
sudo cp ./target/release/dashboard /usr/local/bin/bbb_dashboard


echo "setting up systemd services..."
USER=$(logname)

# === Create gpio.service ===
cat <<EOF | sudo tee /etc/systemd/system/bbb_gpio.service > /dev/null
[Unit]
Description=BBB-gpio Service (runs as user root)
After=network.target

[Service]
ExecStart=$gpio_path
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF



sudo cat <<EOF | sudo tee /usr/lib/systemd/user/$signal_processing.service  > /dev/null
[Unit]
Description=BBB Signal Processing Service
After=default.target

[Service]
ExecStart=/usr/local/bin/bbb_sp
Restart=on-failure
Environment="PULSE_RUNTIME_PATH=/run/user/%U/pulse/"
Environment="XDG_RUNTIME_DIR=/run/user/%U"

[Install]
WantedBy=default.target
EOF


# === Reload systemd and enable services ===
echo "[*] Reloading systemd..."
sudo systemctl daemon-reload

echo "[*] Enabling services to start at boot..."
sudo systemctl enable $gpio
echo "[*] ... GPIO service enabled."

# Enable and start user service (via su to run as the user)
sudo loginctl enable-linger $CURRENT_USER
systemctl --user daemon-reexec
systemctl --user daemon-reload
systemctl --user enable $signal_processing
echo "[*] ... Signal Processing service enabled."

# === Optional: Start the services now ===
read -p "Do you want to start the services now? (y/n): " choice
if [[ "$choice" =~ ^[Yy]$ ]]; then
    sudo systemctl start $gpio
    systemctl --user start $signal_processing
    echo "[*] Services started."

    systemctl --user status $signal_processing
    systemctl status $gpio
else
    echo "[*] Services not started. You can start them manually with:"
    echo "    systemctl --user start $signal_processing"
    echo "    sudo systemctl start $gpio"


fi
