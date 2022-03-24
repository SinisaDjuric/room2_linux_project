#!/bin/bash

# Log file name
log_file="out_log.txt"

# Remove old log file if exists
if ls -l | grep $log_file &> /dev/null ; then
    rm $log_file
fi

# Clean
make clean

# Build
make
