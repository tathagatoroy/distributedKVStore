#!/bin/bash

# This script launches the server and client executables, each in a new terminal window.
# It is intended to be run from a bash shell on Windows (like Git Bash).

echo "Starting server in a new terminal..."
# The 'start' command opens a new default terminal (cmd.exe) and runs the specified program.
# The executable is assumed to be in the current directory.
start ./build/Debug/server.exe

# Wait for 2 seconds to give the server time to start up and begin listening for connections.
echo "Waiting for 2 seconds..."
sleep 2

echo "Starting client in a new terminal..."
start ./build/Debug/client.exe

echo "Both server and client have been launched."