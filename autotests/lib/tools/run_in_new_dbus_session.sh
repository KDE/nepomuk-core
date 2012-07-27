#!/bin/bash

# start the new dbus session bus and export the env variables it creates
echo "Starting new DBus session..."
eval `dbus-launch --sh-syntax --exit-with-session`

# start the process that is to be run in this new session
# eval $@
