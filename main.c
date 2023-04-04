#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/msg.h>

int shmkey = 112369;
int msgkey = 963211;
bool signalReceived = false;
bool stillChildrenRunning = true;

typedef struct messageQ
{
    long mtype;
    int quantum;
}messageQ;

typedef struct PCB
{
  int occupied;			// either true or false
  pid_t pid;			// PID of this child
  int startSeconds;		// time this was forked (seconds)
  int startNano;		// time this was forked (nano seconds)
} PCB;

void helpFunction ()
{
  printf
    ("The function call should go as follows: ./oss [-h] [-n proc] [-s simu] [-t timeLimit] ");
  printf
    ("where -h displays this help function and terminates, proc is the number of processes you want to run, simu is the number of simultaneous");
  printf
    ("processes that run and timeLimit is approximately the time limit each child process runs.");
}

void incrementClock(int* shmSec, int* shmNano, int addNano){
    *shmNano+=addNano;;
    if (*shmNano >= 1000000){
    *shmNano = 0;
    *shmSec+=1;
    }
}

void forker (int* totalLaunched, int* PCB_INDEX, PCB * processTable, messageQ msg, int* shmSec, int* shmNano, int message_queue)
{
  pid_t pid; 
  
    if (*totalLaunched == 5)
    {
      return;
    }
  else
    {
      if ((pid = fork ()) < 0) // FORK HERE
	{
	  perror ("fork");
	}
      else if (pid == 0)
	{
        // CALL TO EXEC CHILD PROCESS
        /*
        char* args[]={"./worker",0};
        execlp(args[0],args[0],args[1]);
        */
        
        /*CONTENTS OF WORKER FILE*/
        messageQ buff;
        buff.mtype = getppid();
        buff.quantum = 10;
        if(msgrcv(message_queue,&msg,sizeof(msg)-sizeof(long),getpid(),0)== -1){
            perror("MESSAGE RECEIVED FAILED");
            exit(1);
        }
	    printf("MESSAGE RECEIVED FROM PARENT: QUANTUM RECEIVED = %d\n",msg.quantum);
	    
	    if(msgsnd(message_queue,&buff,sizeof(buff)-sizeof(long),0)==-1){
        perror("Failed to send Message");
        exit(1);
    }
	    exit(0);
        
	}
      else if (pid > 0)
	{
	    // record child into PCB
  processTable[*PCB_INDEX].occupied = 1;
  processTable[*PCB_INDEX].pid = pid;
  processTable[*PCB_INDEX].startSeconds = *shmSec;
  processTable[*PCB_INDEX].startNano = *shmNano;
  
  // Up Launch Count & PCB_INDEX Count
  *PCB_INDEX += 1;
  *totalLaunched +=1;
  
 return;
	}
    }
}

bool checkifChildTerminated(int status, PCB *processTable, int size)
{
    int pid = waitpid(-1, &status, WNOHANG);
    int i = 0;
    if (pid > 0){
    for (i; i < size; i++){
        if (processTable[i].pid == pid)
            processTable[i].occupied = 0; // WILL NEED TO SET ITS READY_QUEUE ARRAY VALUE TO -1 FOR FUTURE QUEUE USAGE
    }
    return true;
    }
    else if (pid == 0)
        return false;
}
void initializeStruct(struct PCB *processTable){
int i = 0;
for (i; i < 18; i++){
processTable[i].occupied = 0;
processTable[i].pid = 0;
processTable[i].startSeconds = 0;
processTable[i].startNano = 0;
}
}

void initializeReadyQueue(int* READY_QUEUE, int SIZE){
    int i = 0;
    for(i;i<SIZE;i++){
        READY_QUEUE[i] = -1;
    }
}

int PCB_Has_Room(struct PCB *processTable){
    int i = 0;
    for (i;i<18; i++){
        if (processTable[i].occupied == 0) // if our PCB has a non occupied slot give it to a needy process
            return i;
    }
    return -1;
}

void INSERT_INTO_QUEUE (int* READY_QUEUE, struct PCB *processTable, int* PCB_INDEX){
    int i = 0;
    for (i; i<18; i++){
        if (READY_QUEUE[i] == -1) // First available spot in READY_QUEUE, place PID of process HERE
            READY_QUEUE[i] = processTable[*PCB_INDEX].pid;
    }
}


void printStruct (struct PCB *processTable,int* shmSec,int* shmNano, FILE* fLog)
{
    //now write into file
    int j = 0;
  fprintf(fLog,"OSS PID: %d SysClock: %d SysclockNano: %d\n", getpid(),*shmSec,*shmNano);
  fprintf (fLog, "Process Table: \n");
  fprintf (fLog, "ENTRY  OCCUPIED  PID  STARTS  STARTN\n");
    for (j; j < 18; j++)
    {
      fprintf (fLog, "%d        %d       %d    %d        %d\n", j,
	      processTable[j].occupied, processTable[j].pid,
	      processTable[j].startSeconds, processTable[j].startNano);
    }
}

void sig_handler(int signal){
printf("\n\nSIGNAL RECEIVED, TERMINATING PROGRAM\n\n");
stillChildrenRunning = false;
signalReceived = true;
}

void sig_alarmHandler(int sigAlarm){
printf("TIMEOUT ACHIEVED. PROGRAM TERMINATING\n");
stillChildrenRunning = false;
signalReceived = true;
}

char *x = NULL;
char *y = NULL;
char *z = NULL;
char *LogFile = NULL;


int main (int argc, char **argv)
{
  int option;
  while ((option = getopt (argc, argv, "f:h")) != -1)
    {
      switch (option)
	{
	case 'h':
	  helpFunction ();
	  return 0;
	case 'f':
	  LogFile = optarg;
	  break;	    
	}
    }
 
  
    //INITIALIZE ALL VARIABLES
  int totalLaunched = 0;
  int simulLimit = 1;	
  int PCB_INDEX = 0;	
  int status; 
  int exCess;
  int *shmSec;
  int *shmNano;
  PCB processTable[18];
  int READY_QUEUE[18];
  messageQ msg;
  messageQ rcvbuf;
  bool initialLaunch = false;
  
 //create file for LOGFILE
 FILE* fLog;
 fLog = fopen("LogFile", "w");
 if (fLog == NULL)
 {
     printf("ERROR: Could not open file\n");
     return 1;
 }
 
 // signal handlers
 signal(SIGINT, sig_handler);
 signal(SIGALRM, sig_alarmHandler);
 alarm(60); // break at 60 seconds
 
 // Set up message queue
 int message_queue;
 if(( message_queue = msgget(msgkey, 0777 | IPC_CREAT)) == -1){
 perror("MSGET");
 exit(1);
  }
  if (message_queue == -1)
  {
    printf ("ERROR IN MESSAGE QUEUE\n");
    exit(0);
  }
  
  //set up shared memory
  int shmid = shmget (shmkey, 2 * sizeof (int), 0777 | IPC_CREAT);	// create shared memory
  if (shmid == -1)
    {
      printf ("ERROR IN SHMGET\n");
      exit (0);
    }
    
    //Clock
  shmSec = (int *) (shmat (shmid, 0, 0)); // create clock variables
  shmNano = shmSec + 1;
  *shmSec = 0; // initialize clock to zero
  *shmNano = 0;
   int timeLastChildwasLaunchedNano = 0;
   int timeLastChildwasLaunchedSec = 0;
 // initialize struct to 0
 initializeStruct(processTable);
 initializeReadyQueue(READY_QUEUE,18);
   while(stillChildrenRunning){
  // FORK CHILDREN 
    incrementClock(shmSec,shmNano, 2000);
    if (*shmNano >= timeLastChildwasLaunchedNano + 2000 && *shmSec >= timeLastChildwasLaunchedSec){// if enough time has passed to launch a new child, do it
        timeLastChildwasLaunchedNano = *shmNano; // reset counter variables
        timeLastChildwasLaunchedSec = *shmSec;
        
        PCB_INDEX = PCB_Has_Room(processTable);
        if (PCB_INDEX >= 0){ // If theres an unoccupied slot in PCB, Launcha child and place it in Ready Queue
            forker (&totalLaunched, &PCB_INDEX, processTable, msg, shmSec, shmNano, message_queue);
            INSERT_INTO_QUEUE (READY_QUEUE, processTable ,&PCB_INDEX);
        }
    }
    
    msg.mtype = processTable[(PCB_INDEX - 1)].pid; // sets message type to pid of last lauched child
    msg.quantum = 5000; // gives child Quantum to run
    
    checkifChildTerminated(status, processTable, 19);
    
    printStruct (processTable, shmSec, shmNano, fLog);
    
    if (totalLaunched == 5)
            stillChildrenRunning = false;
    }
    
if (signalReceived == true){ // KILL ALL CHILDREN IF SIGNAL
int i = 0;
pid_t childPid;
for (i;i<20;i++){
	if (processTable[i].pid > 0 && processTable[i].occupied == 1){ // IF there is a process who is still runnning
		childPid = processTable[i].pid;
		kill(childPid, SIGKILL);
	}
}
}

  //DETACH SHARED MEMORY
  shmdt (shmSec);
  shmctl (shmid, IPC_RMID, NULL);
  // DETACH MESSAGE QUEUE
  if(msgctl(message_queue, IPC_RMID, NULL) == -1) {
  perror("MSGCTL");
  exit(1);
  }
  return (0);
}




