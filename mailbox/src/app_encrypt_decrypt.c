
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <signal.h>
#define MAX_BUFFER 50
#define MAX_STRING MAX_BUFFER - 2
#define CONST 5

static sem_t sem_encrypt;
static sem_t sem_decrypt;
static sem_t sem_finish;
static sem_t sem_stop;

static sigset_t sigset;  

void wait_next_activation(void)
{
    /* Wait for the signal from the sigset and write in the dummy data to
       the signal that came */
    int dummy;
    sigwait(&sigset, &dummy); 
}

int start_periodic_timer(long offs, int period)
{
    struct itimerval t;
    int ret;

    t.it_value.tv_sec = offs / 1000000;
    t.it_value.tv_usec = offs % 1000000;
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_usec = period % 1000000;
    /* Initialization */
    sigemptyset(&sigset);
    /* Add SIGALRM in set sigset */
    sigaddset(&sigset, SIGALRM);
    /* Setting which signal from sigset is monitored */
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    ret = setitimer(ITIMER_REAL, &t, NULL);
    return ret;
}

char getch(void);

void* create_msg(void* param)
{
    int ret;
    int mode;
    int i;
    char message[MAX_BUFFER];
    int file_descriptor = *(int*)(param);
    /* Length of the message created */
    int len = random()%(MAX_STRING);  

    while(1)
    {
        if (sem_trywait(&sem_finish) == 0)
        {
             break;
        }
        
        for(i =0; i< len; i++)
        {
            /* Creating a random character */
            message[i]= 'A';//+ (random()%26);     
        }
        message[i]='\0'; 

        printf("\ncreated:   %s  sizeof: %d\n",message , sizeof(message));
        /* Set mode to encryption */
        mode = 1;
        if (ioctl(file_descriptor, mode)) 
        {
            printf("Error sending the ioctl command %d to file\n", mode);
            exit(1);
        }

        /* Encrypt data using write */
        ret = write(file_descriptor, message, sizeof(message));

        if(ret < 0)
        {
            printf("Create msg thread: failed to write data to driver\n");
            //return -1;
        }
         sem_post(&sem_encrypt);
         wait_next_activation();
    }
    return 0;
}

void* encrypted_msg(void* param)
{
    int mode;
    int ret;
    char encrypted[MAX_BUFFER];
    int file_descriptor = *(int*)(param);
    
    while(1)
    {
        if (sem_trywait(&sem_finish) == 0)
        {
             break;
        }
        if (sem_trywait(&sem_encrypt) == 0)
        {
            /* Read encrypted message */
            ret = read(file_descriptor, encrypted, sizeof(encrypted));
            if( ret<0)
            {
                printf("Encrypted thread: failed to read data from driver\n");
                //return -1;
            }
            printf("Encrypted: %s  sizeof: %d\n", encrypted + 2, sizeof(encrypted));
            
            int rand = random()%10;

            if (rand < 3)
            {
                /* Change random encrypted message to test decryption */
                encrypted[2]++;
                printf("Encrypted with change: %s  sizeof: %d\n", encrypted + 2, sizeof(encrypted));
            }
            mode = 0;
            if (ioctl(file_descriptor, mode)) 
            {
                printf("Error sending the ioctl command %d to file\n", mode);
                exit(1);
            }

            /* Decrypt data using write */
            ret = write(file_descriptor, encrypted, sizeof(encrypted));
            if (ret < 0)
            {
                printf("Encrypted thread: failed to write data to driver\n");
                //return -1;
            }
            sem_post(&sem_decrypt);
        }
    }
    return 0;
}
void* decrypted_msg(void* param)
{
    int ret;
    char decrypted[MAX_BUFFER];
    int file_descriptor = *(int*)(param);

    while(1)
    {
        if (sem_trywait(&sem_finish) == 0)
        {
             break;
        }
        if (sem_trywait(&sem_decrypt) == 0)
        {
            ret = read(file_descriptor, decrypted, sizeof(decrypted));
            if(ret < 0)
            {
                printf("Decrypted thread: failed to read data from driver\n");
                //return -1;
            }

            printf("Decrypted: %s  sizeof: %d\n", decrypted, sizeof(decrypted));
            /* Check if sending should stop. */
            if (sem_trywait(&sem_stop) == 0)
            {
                /* Terminate threads. Signal the semaphore four times
                in order to notify all four threads. */
                sem_post(&sem_finish);
                sem_post(&sem_finish);
                sem_post(&sem_finish);
                sem_post(&sem_finish);
            }
        }
    }
    return 0;
}
    
void* isFinish(void* param)
{
    char c;

    while(1)
    {
        if (sem_trywait(&sem_finish) == 0)
        {
            break;
        }

        c = getch();
        if(c =='q' || c =='Q')
        {
            sem_post(&sem_stop);
        }
        usleep(50000);
                
    }
    return 0;
}

int main(int argc, char* argv[])
{
    pthread_t thread_1;
    pthread_t thread_2;
    pthread_t thread_3;
    pthread_t thread_finish;

    int ret;
    int file_descriptor;

    if (argc < 2) 
    {
        printf("Wrong number of arguments!\n");
        exit(1);
    }

    /* Open driver */
    file_descriptor = open(argv[1], O_RDWR);

    if (file_descriptor < 0) 
    {
        printf("File '%s' can't be opened!\n", argv[1]);
        exit(1);
    }

    printf(" Press q or Q to finish program!\n");

    /* Initialize semaphores */
    /* Signal the first thread */
    ret = sem_init(&sem_encrypt, 0, 0);
    if (ret != 0)
    {
        printf("Error, semaphore sem_encrypt is not initialised!\n");
        return -1;
    }
    ret = sem_init(&sem_decrypt, 0, 0);
    if(ret != 0)
    {
        printf("Error, semaphore sem_decrypt is not initialised!\n");
        return -1;
    }
    ret = sem_init(&sem_finish, 0, 0);
    if(ret != 0)
    {
        printf("Error, semaphore sem_finish is not initialised!\n");
        return -1;
    }
    ret = sem_init(&sem_stop, 0, 0);
    if(ret != 0)
    {
        printf("Error, semaphore sem_stop is not initialised!\n");
        return -1;
    }

    /* Create threads */
    ret = pthread_create(&thread_1, NULL, create_msg, &file_descriptor);
    if(ret != 0)
    {
        printf("Error, thread_1 is not created!\n");
        return -1;
    }
    ret = pthread_create(&thread_2, NULL, encrypted_msg, &file_descriptor);
    if(ret != 0)
    {
        printf("Error, thread_21 is not created!\n");
        return -1;
    }
    ret = pthread_create(&thread_3, NULL, decrypted_msg, &file_descriptor);
    if(ret != 0)
    {
        printf("Error, thread_3 is not created!\n");
        return -1;
    }
    ret = pthread_create(&thread_finish, NULL, isFinish, 0);
    if(ret != 0)
    {
        printf("Error, thread_finish is not created!\n");
        return -1;
    }

    /* Start periodic timer with offset 2s and period 2s */
    ret = start_periodic_timer(2000000, 2000000);
    if (ret < 0) 
    {
        perror("Start Periodic Timer");
        return -1;
    }

    /* Join threads */
    ret = pthread_join(thread_1, NULL);
    if(ret != 0)
    {
        printf("Error, thread_1 is not joined!\n");
        return -1;
    }
    ret = pthread_join(thread_2, NULL);
    if(ret != 0)
    {
        printf("Error, thread_2 is not joined!\n");
        return -1;
    }
    ret = pthread_join(thread_3, NULL);
    if(ret != 0)
    {
        printf("Error, thread_3 is not joined!\n");
        return -1;
    }

    /* Destroy semaphores */
    ret = sem_destroy(&sem_encrypt);
    if(ret != 0)
    {
        printf("Error, failed to destroy semaphore sem_encrypt!\n");
        return -1;
    }
    ret = sem_destroy(&sem_decrypt);
    if(ret != 0)
    {
        printf("Error, failed to destroy semaphore sem_decrypt!\n");
        return -1;
    }
    ret = sem_destroy(&sem_finish);
    if(ret != 0)
    {
        printf("Error, failed to destroy semaphore sem_finish!\n");
        return -1;
    }
    ret = sem_destroy(&sem_stop);
    if(ret != 0)
    {
        printf("Error, failed to destroy semaphore sem_stop!\n");
        return -1;
    }

    /* Close driver */
    ret = close(file_descriptor);
    if(ret != 0)
    {
        printf("Error, failed to close the file!\n");
        return -1;
    }
    
    return 0;    
}

