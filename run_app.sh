#!/bin/bash

#module device file
module_file_name=/dev/mailbox_module

# Log file name
log_file="out_log.txt"

# App path
app_dir="mailbox/build"

# App name
app_name="app"

# Run app
./$app_dir/$app_name $module_file_name | tee $log_file
