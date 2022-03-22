// C program to implement
// the above approach
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int main (int argc, char *argv[])
{
    int fileDeskriptor;
    char encrypted[50];
    char decrypted[50];
    char message[] = "pero";
    int mode = 0; //decryption mode

    /* app takes as argument path to the device file 
    example of calling app is: ./app/test_app /dev/project_embedded */
    if (argc < 2) {
        printf("Wrong number of arguments!\n");
        exit(1);
    }
 
    // Opening file in reading/writting mode
    fileDeskriptor = open(argv[1], O_RDWR);
    if (fileDeskriptor < 0) {
        printf("File '%s' can't be opened!\n", argv[1]);
        exit(1);
    }

    /* set mode to encryption */
    mode = 1;
	if (ioctl(fileDeskriptor, mode)) {
		printf("Error sending the ioctl command %d to file %s\n", mode, argv[1]);
		exit(1);
  	}
    /* encrypt data using write */
    write(fileDeskriptor, message, sizeof(message));
    /* get encrypted data using read */
    read(fileDeskriptor, encrypted, sizeof(encrypted));
    /* print encrypted data */
    printf("encrypted = '%s'\n", encrypted + 2); //has to be +2 because lenth and CRC are on first two possitions due to encryption mode


    /* set mode to decryption */
    mode = 0;
	if (ioctl(fileDeskriptor, mode)) {
		printf("Error sending the ioctl command %d to file %s\n", mode, argv[1]);
		exit(1);
  	}
    /* decrypt data using write */
    write(fileDeskriptor, encrypted, sizeof(encrypted));
    /* get decrypted data using read */
    read(fileDeskriptor, decrypted, sizeof(decrypted));
    /* print decrypted data */
    printf("decrypted = '%s'\n", decrypted);
 
    // Closing the file
    close(fileDeskriptor);
    return 0;
}