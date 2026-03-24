#!/bin/bash
# MilkyWayOS Auto-Start Script
# Place in /usr/local/bin/milkywayos-session
# This is launched automatically on boot via autologin

echo "==================================="
echo "  Welcome to MilkyWayOS"
echo "  Starting Nebula Desktop..."
echo "==================================="

sleep 1

# Start Starlight (which includes Nebula desktop)
exec /usr/local/bin/starlight
