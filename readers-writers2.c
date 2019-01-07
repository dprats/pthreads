#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>


//CONSTANTS
#define NUM_READERS 5
#define NUM_WRITERS 5
#define NUM_READS 5
#define NUM_WRITES 5
#define MAX_SIMULTANEOUS_READERS 1

//GLOBAL VARIABLES
void *reader(void *param);
void *writer(void *param);

//globaly shared variable
int gSharedValue = 0;
//proxy shared memory lock we will use to enter and exit critical sections
pthread_mutex_t gSharedMemoryLock = PTHREAD_MUTEX_INITIALIZER;
//conditional variables we will use for broadcasting and signaling
pthread_cond_t gReadPhase = PTHREAD_COND_INITIALIZER;
pthread_cond_t gWritePhase = PTHREAD_COND_INITIALIZER;
//number of readers waiting
int gWaitingReaders = 0;
//number of readers accessing variable
int gReaders = 0;


int main(int argc, char *argv[]){

	int i = 0;
	//array of threads
	pthread_t readers[NUM_READERS];
	pthread_t writers[NUM_READERS];
	int readerIds[NUM_READERS];
	int writerIds[NUM_WRITERS];

	//generate the reader threads
	for (i = 0; i < NUM_READERS; i++) {
		//create one reader thread
		readerIds[i] = i * 100;
		pthread_create(&readers[i], NULL, reader, &readerIds[i]);
	}

	//generate the writer threads
	for (i = 0; i < NUM_WRITERS; i++){
		writerIds[i] = i * 1000;
		pthread_create(&writers[i], NULL, writer, &writerIds[i]);
	}

	//join the reader threads
	for (i = 0; i < NUM_READERS; i++){
		pthread_join(readers[i], NULL);
		printf("\tclosed thread r[%d]\n", readerIds[i]);
	}

	//join the writer threads
	for (i = 0; i < NUM_WRITERS; i++){
		pthread_join(writers[i], NULL);
		printf("\tclosed thread w[%d]\n", writerIds[i]);
	}

	printf("parent exiting\n");
	return 0;
}

void *reader(void *param){

	int readerId = *((int *) param);
	printf("creating reader with id: r[%d]\n", readerId);
	int i = 0;

	for (i = 0; i < NUM_READS; i++) {
		
		// Wait so that reads and writes do not all happen at once
	  	usleep(1000 * (random() % NUM_READERS + NUM_WRITERS));

		//enter critical sections
		pthread_mutex_lock(&gSharedMemoryLock);
			gWaitingReaders++;
			while (gReaders > MAX_SIMULTANEOUS_READERS){
				pthread_cond_wait(&gReadPhase, &gSharedMemoryLock);
			}
			//if we reach here, we passed while loop and we have a thread no longer waiting
			gWaitingReaders--; 
			gReaders++;
		pthread_mutex_unlock(&gSharedMemoryLock);

		//read
		printf("[r-%d] reading '%u' for the nth(%d) time [readers: %2d]\n", readerId, gSharedValue, i + 1, gReaders);

		//exit critical section
		pthread_mutex_lock(&gSharedMemoryLock);
			gReaders--;
			if (gReaders == 0) {
				pthread_cond_signal(&gWritePhase);
			}
		pthread_mutex_unlock(&gSharedMemoryLock);
	}

	pthread_exit(0);
}

void *writer(void *param){
	
	int writerId = *((int *) param);
	int i = 0;
	printf("creating writer with id: w[%d]\n", writerId);

	for (i = 0; i < NUM_WRITES; i++) {

		// Wait so that reads and writes do not all happen at once
	  	usleep(1000 * (random() % NUM_READERS + NUM_WRITERS));


		//enter citical section
		pthread_mutex_lock(&gSharedMemoryLock);
			while(gReaders > 0) {
				pthread_cond_wait(&gWritePhase, &gSharedMemoryLock);
			}
			gReaders = -1;
		pthread_mutex_unlock(&gSharedMemoryLock);

		//write
		gSharedValue++;
		printf("[w-%d] writing '%u' for the nth(%d) time [readers: %2d]\n", writerId, gSharedValue, i + 1, gReaders);

		//exit critical code
		pthread_mutex_lock(&gSharedMemoryLock);
			gReaders = 0;
			if (gWaitingReaders > 0) {
				pthread_cond_broadcast(&gReadPhase);
			} else {
				pthread_cond_signal(&gWritePhase);
			}
		pthread_mutex_unlock(&gSharedMemoryLock);
	}

	pthread_exit(0);
}