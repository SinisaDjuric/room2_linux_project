Guidance for running app and loading kernel module

1. Clone code to /home/USER_NAME/linux-kernel-labs/modules/nfsroot/root/
2. Position yourself inside room2_linux_project directory on nativ Linux
3. Run "sh clean_build.sh" from nativ linux terminal to remove all build and log files
4. Run picocom from nativ linux terminal (picocom  -b  115200  /dev/ttyUSB0 for example)
5. Enter room2_linux_project on Raspberry pi
6. Run "sh load_module.sh" from Raspberry pi terminal to insert module and create device nod
7. Run "sh run_app.sh" from Raspberry pi terminal to run application