#!/bin/bash

# Clean
make clean

# Build
make

# Name of the module
kernel_module_dir="kernel_module"

# Name of the module
module="mailbox_module"

# Name of the device
device="mailbox_module"

# Name of the directory the script is placed in
dir_name=$(dirname "$0")

# Path to script and kernel module
curr_dir=$(realpath ${dir_name})

# Remove old module if exists
if lsmod | grep $module &> /dev/null ; then
    rmmod $module
fi

# Load module to kernel
insmod $curr_dir/$kernel_module_dir/$module.ko mode=0

# Get major number
major=$(cat /proc/devices | grep $module | cut -d' ' -f1)

# Link driver with device file
mknod -m 666 /dev/$device c $major 0

