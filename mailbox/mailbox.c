#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#define BUFF_MAX_SIZE (50)
#define SLEEPING_TIME (2000000)
#define ENCRYPT       (1)
#define DECRYPT       (0)

char getch(void);


static sem_t semEncrypted;
static sem_t semDecrypted;
static sem_t semFinishSignal;
static sem_t semSent;

static pthread_mutex_t deviceAccess;

static int file;
static int data_len;
static int crc_len;
static char file_name[] = "/dev/mailbox";

/* encrypter thread routine. */
void* encrypter (void *param)
{
    int key;
    int i;
	int ret_val = 0;
    char data[BUFF_MAX_SIZE + 1];
    static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    int err_inj = *(int*)param;

    while (1)
    {
        if (sem_trywait(&semFinishSignal) == 0)
        {
            break;
        }

        if (sem_trywait(&semSent) == 0)
        {
            /* Wait 2s. */
            usleep(SLEEPING_TIME);

            data_len = rand() % BUFF_MAX_SIZE;

            for (i = 0; i < data_len; i++)
            {
                key = rand() % (int)sizeof(charset);
                data[i] = charset[key];
            }
            data[i] = '\n';

            pthread_mutex_lock(&deviceAccess);
            if (ioctl(file, ENCRYPT, err_inj)) {
                fprintf(stderr, "Error sending the ioctl command %d to file %s\n", ENCRYPT, file_name);
            }
            /* Write to /dev/mailbox device. */
            ret_val = write(file, data, data_len);
            pthread_mutex_unlock(&deviceAccess);

            sem_post(&semEncrypted);
        }
    }

    return 0;
}

/* decrypter thread routine. */
void* decrypter (void *param)
{
	int ret_val = 0;
    char data[BUFF_MAX_SIZE + 1];
    int err_inj = *(int*)param;
    while (1)
    {
        if (sem_trywait(&semFinishSignal) == 0)
        {
            break;
        }

        if (sem_trywait(&semEncrypted) == 0)
        {
            /* Access the ring buffer. */
            pthread_mutex_lock(&deviceAccess);
            /* Read from /dev/encrypter device. */
            ret_val = read(file, data, data_len + crc_len);

            if (ioctl(file, DECRYPT, err_inj)) {
                fprintf(stderr, "Error sending the ioctl command %d to file %s\n", DECRYPT, file_name);
            }

            pthread_mutex_unlock(&deviceAccess);
            printf("Encrypted message: %s", data);


            sem_post(&semDecrypted);
        }
    }

    return 0;
}

/* sender thread routine. */
void* sender (void *param)
{
	int ret_val = 0;
    char data[BUFF_MAX_SIZE + 1];
    while (1)
    {
        if (sem_trywait(&semFinishSignal) == 0)
        {
            break;
        }

        if (sem_trywait(&semDecrypted) == 0)
        {
            /* Access the ring buffer. */
            pthread_mutex_lock(&deviceAccess);
            /* Read from /dev/encrypter device. */
            ret_val = read(file, data, data_len + crc_len);

            pthread_mutex_unlock(&deviceAccess);

            printf("Decrypted message: %s", data);
            /* Terminate thread; Signal the semaphore three times
            in order to notify all three threads. */
            sem_post(&semFinishSignal);
            sem_post(&semFinishSignal);
            sem_post(&semFinishSignal);
        }
    }

    return 0;
}
            
/* Main thread creates two additinoal threads (the encrypter and the decrypter) and waits them to terminate. */
int main (int argc, char *argv[])
{
    /* Thread IDs. */
    pthread_t hEncrypter;
    pthread_t hDecrypter;
    pthread_t hSender;

	if (argc != 2) {
		fprintf(stderr, "mailbox: wrong number of arguments;\n");
		fprintf(stderr, "1st argument: error injection {0 | 1} \n");
		return -1;
	}

    /* Create semEncrypted, semDecrypted, semSent and semFinishSignal semaphores. */
    sem_init(&semEncrypted, 0, 0);
    sem_init(&semDecrypted, 0, 0);
    sem_init(&semSent, 0, 1);
    sem_init(&semFinishSignal, 0, 0);

    /* Initialise mutex. */
    pthread_mutex_init(&deviceAccess, NULL);
    if (file = open("/dev/mailbox", O_RDWR) < 0){
		fprintf(stderr, "Error opening file %s\n", argv[1]);
		return -1;
  	}
    /* Create threads: the encrypter, the decrypter and the sender. */
    pthread_create(&hEncrypter, NULL, encrypter, argv[1]);
    pthread_create(&hDecrypter, NULL, decrypter, 0);
    pthread_create(&hSender, NULL, sender, 0);

    /* Join threads (wait them to terminate) */
    pthread_join(hDecrypter, NULL);
    pthread_join(hEncrypter, NULL);
    pthread_join(hSender, NULL);

    /* Release resources. */
    sem_destroy(&semEncrypted);
    sem_destroy(&semDecrypted);
    sem_destroy(&semSent);
    sem_destroy(&semFinishSignal);
    pthread_mutex_destroy(&deviceAccess);
    /* Close /dev/mailwox device. */
    close(file);
    printf("\n");

    return 0;
}
