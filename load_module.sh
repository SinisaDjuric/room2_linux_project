#!/bin/bash
#name of the driver (module)
module=project_embedded
#name of the driver file
device=project_embedded
rm -f /dev/$device #remove old device file
insmod $module.ko     #insert module to kernel
major=`cat /proc/devices | grep project_embedded | cut -d' ' -f1` #get major number
mknod /dev/$device -m 666 c $major 0 #link driver with device file