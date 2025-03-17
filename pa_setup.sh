# adding hfgw=false in the /etc/pulse/default.pa file after the line load-module module-bluetooth-discover
# this will work but you need to do it with sudo
# sed -i 's/^load-module module-bluetooth-discover$/load-module module-bluetooth-discover hfgw=false/' /etc/pulse/default.pa
# module-bluetooth-discover disables auto create of loopback modules.
# this is desired since we are creating the loopback module manually with the rust code.

#start pulseaudio
# echo "...starting pulseaudio"
# pulseaudio --start