#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "timer_event.h"

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

static struct termios old, new;

/* Initialize new terminal i/o settings */
void initTermios(int echo) 
{
    tcgetattr(0, &old); /* grab old terminal i/o settings */
    new = old; /* make new settings same as old settings */
    new.c_lflag &= ~ICANON; /* disable buffered i/o */
    new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
    tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void) 
{
    tcsetattr(0, TCSANOW, &old);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo) 
{
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

/* Read 1 character without echo */
char getch(void) 
{
    return getch_(0);
}

/* Read 1 character with echo */
char getche(void) 
{
    return getch_(1);
}
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
    char c;

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
            /* Get char when keypressed. */
            c = getch();

            /* In case of q or Q char, signal both
               threads (including this one) to terminate. */
            if (c == 'q' || c == 'Q')
            {
                /* Terminate thread; Signal the semaphore three times
                in order to notify all three threads. */
                sem_post(&semFinishSignal);
                sem_post(&semFinishSignal);
                sem_post(&semFinishSignal);
            }
        }
    }

    return 0;
}

/* Timer callback being caller on every 2 seconds. */
void* send_new_mail (void *param)
{
    sem_post(&semSent);

    return 0;
}


/* Main thread creates two additinoal threads (the encrypter and the decrypter) and waits them to terminate. */
int main (int argc, char *argv[])
{
    /* Thread IDs. */
    pthread_t hEncrypter;
    pthread_t hDecrypter;
    pthread_t hSender;

    /* Timer ID. */
    timer_event_t hPrintStateTimer;


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
    if ((file = open("/dev/mailbox", O_RDWR)) < 0){
        fprintf(stderr, "Error opening file %s\n", argv[1]);
        return -1;
      }
    /* Create threads: the encrypter, the decrypter and the sender. */
    pthread_create(&hEncrypter, NULL, encrypter, argv[1]);
    pthread_create(&hDecrypter, NULL, decrypter, 0);
    pthread_create(&hSender, NULL, sender, 0);

    /* Create the timer event for send_new_mail callback function. */
    timer_event_set(&hPrintStateTimer, 2000, send_new_mail, 0, TE_KIND_REPETITIVE);

    /* Join threads (wait them to terminate) */
    pthread_join(hDecrypter, NULL);
    pthread_join(hEncrypter, NULL);
    pthread_join(hSender, NULL);

    /* Stop the timer. */
    timer_event_kill(hPrintStateTimer);

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
