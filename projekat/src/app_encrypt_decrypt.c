
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
static sem_t sem_dectypt;
static sem_t semFinishSignal;

static sigset_t sigset;  

void wait_next_activation(void)
{
	/* waits for the signal from the sigset and writes in the dummy
           the signal that came */
	int dummy;
	sigwait(&sigset, &dummy); 
}

int start_periodic_timer(long offs, int period)
{
    	struct itimerval t;

    	t.it_value.tv_sec = offs / 1000000;
    	t.it_value.tv_usec = offs % 1000000;
    	t.it_interval.tv_sec = period / 1000000;
   	t.it_interval.tv_usec = period % 1000000;

    	sigemptyset(&sigset);                   /* initialization */
	sigaddset(&sigset, SIGALRM);            /* add SIGALRM in set sigset */
	sigprocmask(SIG_BLOCK, &sigset, NULL);  /* setting which signal from sigset is monitored */
   	return setitimer(ITIMER_REAL, &t, NULL);
}

char getch(void);

void* create_msg(void* param)
{
	int fileDeskriptor = *(int*)(param);
	int ret;
	int mode;
	//printf(" Fajl deskriptor %d\n", fileDeskriptor);
	while(1)
	{
		if (sem_trywait(&semFinishSignal) == 0)
      		{
           		 break;
        	}
		int i;
		int len = random()%(MAX_STRING);   /* length of the message created */
		char message[len];
		
		for( i =0; i< len; i++)
		{
			message[i]= 'A' ;//+ (random()%26); 	/* creating a random character */
		}
		message[i]='\0'; 
	
		printf("\ncreated:   %s  sizeof: %d\n",message , sizeof(message));
	
	        /* set mode to encryption */
	        mode = 1;
		if (ioctl(fileDeskriptor, mode)) {
			printf("Error sending the ioctl command %d to file\n", mode);
			exit(1);
	  	 }
	         /* encrypt data using write */
	        ret = write(fileDeskriptor, message, sizeof(message));
		if( ret<0)
		{
			printf("create msg thread: failed to write data to driver\n");
			//return -1;
		}
		 sem_post(&sem_encrypt);
		 wait_next_activation();
	}
	return 0;
}

void* encrypted_msg(void* param)
{
	int fileDeskriptor = *(int*)(param);
	int mode;
	int ret;
	char encrypted[MAX_BUFFER];
	
	while(1)
	{
		if (sem_trywait(&semFinishSignal) == 0)
      		{
           		 break;
        	}
		if(sem_trywait(&sem_encrypt)==0)
		{
			ret = read(fileDeskriptor, encrypted, sizeof(encrypted)); /* read encrypted message */
			if( ret<0)
			{
				printf("encrypted thread: failed to read data from driver\n");
				//return -1;
			}		
			printf("encrypted: %s  sizeof: %d\n", encrypted + 2, sizeof(encrypted));   
			
			int rand = random()%10;
			if( rand <3)
			{
		  		encrypted[2]++;   /* change random encrypted message to test decryption */
		  		printf("encrypted with change: %s  sizeof: %d\n", encrypted + 2, sizeof(encrypted));
		  	}
			mode = 0;
			if (ioctl(fileDeskriptor, mode)) {
				printf("Error sending the ioctl command %d to file\n", mode);
				exit(1);
		  	}
	
		         /* decrypt data using write */
		        ret = write(fileDeskriptor, encrypted, sizeof(encrypted));
		        if( ret<0)
			{
				printf("encrypted thread: failed to write data to driver\n");
				//return -1;
			}

			sem_post(&sem_dectypt);
		}
	}
	return 0;
}
void* decrypted_msg(void* param)
{
	int fileDeskriptor = *(int*)(param);
	int ret;
	char decrypted[MAX_BUFFER];
	while(1)
	{
		if (sem_trywait(&semFinishSignal) == 0)
      		{
           		 break;
        	}
		if(sem_trywait(&sem_dectypt)==0)
		{
			ret = read(fileDeskriptor, decrypted, sizeof(decrypted));
			if( ret<0)
			{
				printf("decrypted thread: failed to read data from driver\n");
				//return -1;
			}
			printf("decrypted: %s  sizeof: %d\n", decrypted, sizeof(decrypted));  								
		}
	}
	return 0;
}
	
void* isFinish(void* param)
{
	while(1)
	{
		if (sem_trywait(&semFinishSignal) == 0)
        {
            break;
        }
		char c = getch();
		if(c =='q' || c =='Q')
		{
		sem_post(&semFinishSignal);
                sem_post(&semFinishSignal);
                sem_post(&semFinishSignal);
                sem_post(&semFinishSignal);
                }
                usleep(50000);
                
        }
        return 0;
}
int main(int argc, char* argv[]){
	
	pthread_t thread_1;
	pthread_t thread_2;
	pthread_t thread_3;
	pthread_t thread_finish;
	
	int ret;
	int fileDeskriptor;
	
	if (argc < 2) {
        printf("Wrong number of arguments!\n");
        exit(1);
    	}  	
    	
    	fileDeskriptor = open(argv[1], O_RDWR);            /* open driver */
		   
	if (fileDeskriptor < 0) {
		printf("File '%s' can't be opened!\n", argv[1]);
		exit(1);
	}
	/*
	struct arguments {
		//char* name
		int fileDeskriptor;
		};
		*/
	
	ret = start_periodic_timer(2000000, 2000000);     /* start periodic timer with 
	                                                   offset 2s and period 2s */
   	if (ret < 0) 
   	{
        	perror("Start Periodic Timer");
        	return -1;
    	}
    	
    	printf(" Press q or Q to finish program!\n");
    	
	ret = sem_init(&sem_encrypt, 0, 0);                /* initialize semaphores */
	if( ret !=0)
	{
		printf("Error, semaphore is not initialised!\n");
		return -1;
	}
	ret = sem_init(&sem_dectypt, 0, 0);
	if( ret !=0)
	{
		printf("Error, semaphore is not initialised!\n");
		return -1;
	}
	ret = sem_init(&semFinishSignal, 0, 0);
	if( ret !=0)
	{
		printf("Error, semaphore is not initialised!\n");
		return -1;
	}
	
	ret = pthread_create(&thread_1, NULL, create_msg, &fileDeskriptor);    /* create threads */
	if( ret !=0)
	{
		printf("Error, thread is not created!\n");
		return -1;
	}
	ret = pthread_create(&thread_2, NULL, encrypted_msg, &fileDeskriptor);
	if( ret !=0)
	{
		printf("Error, thread is not created!\n");
		return -1;
	}
	ret = pthread_create(&thread_3, NULL, decrypted_msg, &fileDeskriptor);
	if( ret !=0)
	{
		printf("Error, thread is not created!\n");
		return -1;
	}
	ret = pthread_create(&thread_finish, NULL, isFinish, 0);
	if( ret !=0)
	{
		printf("Error, thread is not created!\n");
		return -1;
	}
	
	ret = pthread_join(thread_1, NULL);                                /* join threads */
	if( ret !=0)
	{
		printf("Error, thread is not joined!\n");
		return -1;
	}
	ret = pthread_join(thread_2, NULL);
	if( ret !=0)
	{
		printf("Error, thread is not joined!\n");
		return -1;
	}
	ret = pthread_join(thread_3, NULL);
	if( ret !=0)
	{
		printf("Error, thread is not joined!\n");
		return -1;
	}
	
	ret = sem_destroy(&sem_encrypt);                              /* destroy semaphores */
	if( ret !=0)
	{
		printf("Error, failed to destroy semaphore!\n");
		return -1;
	}
	ret = sem_destroy(&sem_dectypt);
	if( ret !=0)
	{
		printf("Error, failed to destroy semaphore!\n");
		return -1;
	}
	ret = sem_destroy(&semFinishSignal);
	if( ret !=0)
	{
		printf("Error, failed to destroy semaphore!\n");
		return -1;
	}
	ret = close(fileDeskriptor);                                /* close driver */
	if( ret !=0)
	{
		printf("Error, failed to close the file!\n");
		return -1;
	}
	
	return 0;	
}

