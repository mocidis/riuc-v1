#!/bin/bash
remote_addr=$1
if [ $# -eq 0 ]; then
	remote_addr="192.168.2.55"
	echo "Using default remote ip address"
elif [ $# -gt 1 ]; then
	echo "Usage:   $0 remote_ip"
	echo "Example: $0 192.168.2.55" 
	exit 1
fi
scp riuc root@$remote_addr:/home/root/
