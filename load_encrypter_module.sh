#!/bin/bash

# Name of the module
module="encrypter"

# Name of the device
device="encrypter"

# Remove old module
rmmod $module 

# Load module to kernel
insmod ./$module.ko

# Get major number
major=($(cat /proc/devices | grep $module)) 

# Link driver with device file
mknod /dev/$device -m 666 c $major 0 
