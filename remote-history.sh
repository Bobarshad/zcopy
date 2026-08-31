#!/bin/bash

SERVER="192.168.10.1"
USER="root"

ssh -o LogLevel=ERROR -o StrictHostKeyChecking=accept-new "$USER@$SERVER" '
    export HISTFILE=/dev/null

    # OpenWrt does not use PAM or limits.conf
    # ulimit -n works normally
    # sysctl.d is not used; configs live in /etc/sysctl.conf or /etc/config/system

    # Example commands:
    cat .ash_history 
#    cat .bash_history
'

