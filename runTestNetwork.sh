#!/bin/bash

# This script launches 5 testNetwork servers, each in a new terminal window.
# It is intended to be run from a bash shell on Windows (like Git Bash).

echo "Starting 5 testNetwork servers..."



# Start 5 servers with IDs 0-4
for i in {0..4}; do
    echo "Starting testNetwork server $i in a new terminal..."
    # The 'start' command opens a new default terminal (cmd.exe) and runs the specified program
    # Each server gets a unique ID (0-4) and will listen on BASE_PORT + ID (4000 + ID)
    start cmd /k "./build/testExecutables/Debug/testNetwork.exe $i"
    
    # Wait a bit between launches to avoid port conflicts during startup
    sleep 1
done

echo "All 5 testNetwork servers have been launched."
echo "Server 0 is listening on port 4000"
echo "Server 1 is listening on port 4001" 
echo "Server 2 is listening on port 4002"
echo "Server 3 is listening on port 4003"
echo "Server 4 is listening on port 4004"
echo ""
echo "Press any key to stop all servers..."
read -n 1

# Optional: Kill all testNetwork processes (uncomment if needed)
# echo "Stopping all testNetwork processes..."
# taskkill //F //IM testNetwork.exe 2>/dev/null
