#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <semaphore.h>

// Defining constants
#define MOUSEFILE1 "/dev/input/event3"
#define MAX_BUFFER 20
#define MAX_MUTEX_NUM 10

// Defining Enumerations
typedef enum TaskType_t {Periodic, Aperiodic} TaskType;
typedef enum MutexJob_t {Lock, Unlock} MutexJob;
typedef enum ThreadStatus_t {Sleeping, Running} ThreadStatus;
typedef enum AppStatus_t {Ending, Executing} AppStatus;

// Defining Mutex
struct mutexData_t
{
	pthread_mutex_t mutex;
	int mutexData;
}mutexData[MAX_MUTEX_NUM];

//Defining Semaphore
struct semData_t
{
	sem_t sem;
	int semData;
}semData[2];


// Defining Structure to be passed to the thread
typedef struct TaskArgs_t
{
	int index;
	TaskType taskType;
	int priority;
	int period;
	int *runTime;
	int numRunTimes;
	MutexJob *mutexJob;
	int numMutexJobs;
	int *mutexNumber;
	int numMutexNumbers;
	int triggerEvent;
}TaskArgs;

// Status of the application and threads
ThreadStatus *threadStatus;
AppStatus appStatus;

//Starting Time of the application
struct timespec startTime;



// Function to read the sample file
void parseFile(char *fileName, TaskArgs **retArgs, int *numJobs, int *execTime)
{
	FILE *file;

	int runTimeBuffer[MAX_BUFFER];
	MutexJob mutexJobBuffer[MAX_BUFFER];
	int mutexNumberBuffer[MAX_BUFFER];
	char stringBuffer[MAX_BUFFER];
	int jobIndex, index;
	char buffer;
	TaskArgs *args;
	file = fopen(fileName, "r");
	if(file == 0)
	{
		printf("File open failed!!!\n");
		exit(1);
	}

	fscanf(file, "%d%c", numJobs, &buffer);
	fscanf(file, "%d%c", execTime, &buffer);
	args = (TaskArgs *) malloc((*numJobs) * sizeof(TaskArgs));
	threadStatus = (ThreadStatus *) malloc((*numJobs)*sizeof(ThreadStatus));

	memset(args, 0, (*numJobs)*sizeof(TaskArgs));

	for(jobIndex=0; jobIndex<(*numJobs); ++jobIndex)
	{
		args[jobIndex].index = jobIndex;
		fscanf(file, "%c", &buffer);
		if(buffer == 'P')
		{
			args[jobIndex].taskType = Periodic;
			fscanf(file, "%d", &args[jobIndex].priority);
			fscanf(file, "%d", &args[jobIndex].period);
		}
		else
		{
			args[jobIndex].taskType = Aperiodic;
			fscanf(file, "%d", &args[jobIndex].priority);
			fscanf(file, "%d", &args[jobIndex].triggerEvent);
		}

		do
		{
			fscanf(file, "%s%c", stringBuffer, &buffer);
			if(stringBuffer[0]=='L')
			{
				mutexJobBuffer[args[jobIndex].numMutexJobs++] = Lock;
				mutexNumberBuffer[args[jobIndex].numMutexNumbers++] = atoi(&stringBuffer[1]);
			}
			else if(stringBuffer[0]=='U')
			{
				mutexJobBuffer[args[jobIndex].numMutexJobs++] = Unlock;
				mutexNumberBuffer[args[jobIndex].numMutexNumbers++] = atoi(&stringBuffer[1]);
			}
			else
			{
				runTimeBuffer[args[jobIndex].numRunTimes++] = atoi(stringBuffer);
			}

		}while(buffer!='\n');

		args[jobIndex].mutexJob = (MutexJob *) malloc(args[jobIndex].numMutexJobs*sizeof(MutexJob));
		args[jobIndex].mutexNumber = (int *) malloc(args[jobIndex].numMutexNumbers*sizeof(int));
		args[jobIndex].runTime = (int *) malloc(args[jobIndex].numRunTimes*sizeof(int));

		for(index = 0; index<args[jobIndex].numMutexJobs; ++ index)
		{
			args[jobIndex].mutexJob[index] = mutexJobBuffer[index];
		}
		for(index = 0; index<args[jobIndex].numMutexNumbers; ++ index)
		{
			args[jobIndex].mutexNumber[index] = mutexNumberBuffer[index];
		}
		for(index = 0; index<args[jobIndex].numRunTimes; ++ index)
		{
			args[jobIndex].runTime[index] = runTimeBuffer[index];
		}

	}
	(*retArgs) = args;
	fclose(file);
}



// The busy loop for the tasks
void busyLoop(int iterations)
{
	int i,j=0;
	for(i=0; i<iterations; ++i)
	{
		j=j+i;
	}
}



//Doing the task Jobs
void taskJobs(TaskArgs *args)
{
	int numRunTimes=0, numMutexJobs=0, numMutexNumbers=0;

	threadStatus[args->index] = Running;
	while((numRunTimes<args->numRunTimes) || (numMutexJobs<args->numMutexJobs)
			|| (numMutexNumbers<args->numMutexNumbers))
	{
		if(numRunTimes<args->numRunTimes)
		{
			busyLoop(args->runTime[numRunTimes++]);
		}

		if((numMutexJobs<args->numMutexJobs) || (numMutexNumbers<args->numMutexNumbers))
		{
			MutexJob job = args->mutexJob[numMutexJobs++];
			int mutexNumber = args->mutexNumber[numMutexNumbers++];
			if(job == Lock)
			{
				pthread_mutex_lock(&mutexData[mutexNumber].mutex);
			}
			else
			{
				pthread_mutex_unlock(&mutexData[mutexNumber].mutex);
			}
		}
	}
	if(appStatus == Ending)
	{
		pthread_exit(NULL);
	}
	else
	{
		threadStatus[args->index] = Sleeping;
	}
}


//Threads for each task
void * taskThread(void *vArgs)
{
	TaskArgs *args = (TaskArgs *) vArgs;


	if(args->taskType == Periodic)
	{
		struct timespec start, request, remain;
		double sleeptime;
		int count=1;
		while (1)
		{
			clock_gettime(CLOCK_MONOTONIC, &start);
			sleeptime = startTime.tv_sec + startTime.tv_nsec * 1e-9 + args->period*1e-3*count;
			count++;
			request.tv_sec = (time_t) sleeptime;
			request.tv_nsec = (__syscall_slong_t) ((sleeptime - request.tv_sec)*1e+9);

			taskJobs(args);
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &request, &remain);

		}
	}
	else
	{
		while(1)
		{
			sem_wait(&semData[args->triggerEvent].sem);
			taskJobs(args);
		}
	}
	return 0;
}



// Thread for reading mouse events
void *eventThread()
{
	int fd;
	struct input_event ie;
	sem_wait(&semData[0].sem);
	sem_wait(&semData[1].sem);
	while(1)
	{
		fd = open(MOUSEFILE1, O_RDONLY);
		while(read(fd, &ie, sizeof(struct input_event)))
		{
			if(ie.type == EV_KEY && ie.code == BTN_LEFT && ie.value == 0)
			{
				sem_post(&semData[0].sem);
			}
			if(ie.type == EV_KEY && ie.code == BTN_RIGHT && ie.value == 0)
			{
				sem_post(&semData[1].sem);
			}
		}
	}
	return 0;
}



// Start all the jobs read from the sample file
void startJobs(int numJobs, TaskArgs *args, pthread_t **retThreadId, pthread_t *retEventThreadId)
{
	pthread_attr_t attr;
	struct sched_param parm;

	pthread_attr_t *threadAttr;
	struct sched_param *threadParam;
	pthread_t *threadId, eventThreadId;
	int i;
	pthread_mutexattr_t mutexAttr;
	pthread_mutexattr_init(&mutexAttr);
	pthread_mutexattr_setprotocol(&mutexAttr, PTHREAD_PRIO_INHERIT);
	sem_init(&semData[0].sem, 0, 1);
	sem_init(&semData[1].sem, 0, 1);

	for(i=0; i<10; ++i)
	{
		pthread_mutex_init(&mutexData[i].mutex, &mutexAttr);
	}

	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

	pthread_attr_getschedparam(&attr, &parm);
	parm.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_attr_setschedparam(&attr, &parm);

	pthread_setschedparam(pthread_self(), SCHED_FIFO, &parm);

	pthread_create(&eventThreadId, NULL, eventThread, NULL);

	threadId = (pthread_t *) malloc(numJobs*sizeof(pthread_t));
	threadAttr = (pthread_attr_t *) malloc(numJobs*sizeof(pthread_attr_t));
	threadParam = (struct sched_param *) malloc(numJobs*sizeof(struct sched_param));

	for(i=0; i<numJobs;++i)
	{
		pthread_attr_init(&threadAttr[i]);
		pthread_attr_setinheritsched(&threadAttr[i], PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&threadAttr[i], SCHED_FIFO);

		pthread_attr_getschedparam(&threadAttr[i], &threadParam[i]);
		threadParam[i].__sched_priority = args[i].priority;
		pthread_attr_setschedparam(&threadAttr[i], &threadParam[i]);

		pthread_create(&threadId[i], &threadAttr[i], taskThread, &args[i]);
		pthread_attr_destroy(&threadAttr[i]);

	}

	(*retThreadId) = threadId;
	(*retEventThreadId) = eventThreadId;
	free((void *) threadAttr);
	free((void *) threadParam);

}


// Main function
int main(int argc, char **argv)
{
	char *fileName;
	int numJobs;
	int execTime;
	TaskArgs *args;
	pthread_t *threadId, eventThreadId;
	double sleepTime;
	struct timespec request, remain;
	clock_gettime(CLOCK_MONOTONIC, &startTime);
	appStatus = Executing;
	int i;

	if (argc !=2)
	{
		printf("\tUSAGE:\n%s <Input_file_name>", argv[0]);
		exit(0);
	}

	fileName = argv[1];
	parseFile(fileName, &args, &numJobs, &execTime);
	sleepTime = startTime.tv_sec + execTime*1e-3 + startTime.tv_nsec*1e-9;
	request.tv_sec = (long) sleepTime;
	request.tv_nsec = (long) ((sleepTime - request.tv_sec) * 1e9);

	startJobs(numJobs, args, &threadId, &eventThreadId);

	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &request, &remain);

	appStatus = Ending;
	pthread_cancel(eventThreadId);
	for(i=0; i<numJobs; ++i)
	{
		if(threadStatus[i] == Sleeping)
		{
			pthread_cancel(threadId[i]);
		}
	}
	for(i=0; i<numJobs; ++i)
	{
		pthread_join(threadId[i], NULL);
	}

	for(i=0; i<numJobs;++i)
	{
		free(args[i].mutexJob);
		free(args[i].mutexNumber);
		free(args[i].runTime);
	}

	free(args);
	free((void *) threadId);

	for(i=0; i<MAX_MUTEX_NUM; ++i)
	{
		pthread_mutex_destroy(&mutexData[i].mutex);
	}
	sem_destroy(&semData[0].sem);
	sem_destroy(&semData[1].sem);

	return 0;

}
