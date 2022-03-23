#!/bin/bash

# Name of the directory the script is placed in
dir_name=$(dirname "$0")

# Path to script and kernel module
curr_dir=$(realpath ${dir_name})

# File name
file_name="out_log.txt"

# App path
app_dir="mailbox/build"

# App name
app_name="app"

# Run app
./$curr_dir/$app_dir/$app_name | tee $file_name

