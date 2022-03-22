#!/bin/bash

# Name of the module
module="mailbox_module"

# Name of the device
device="mailbox_module"

# Remove old module
rmmod $module 

# Load module to kernel
insmod ./$module.ko

# Get major number
major=$(cat /proc/devices | grep $module | cut -d' ' -f1)

# Link driver with device file
mknod -m 666 /dev/$device c $major 0
