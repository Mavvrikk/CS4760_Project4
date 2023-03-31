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
    int timeSec;
    int timeNano;
}messageQ;

typedef struct PCB
{
  int occupied;			// either true or false
  pid_t pid;			// PID of this child
  int startSeconds;		// time this was forked
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

void incrementClock(int* shmSec, int* shmNano){
    *shmNano+=1;
    if (*shmNano >= 1000000){
    *shmSec+=1;
    *shmNano=0;
    }
}

int forker (int totaltoLaunch, int simulLimit, int timeLimit, int* totalLaunched, PCB * processTable, messageQ msg, int* shmSec, int*shmNano,int message_queue)
{
  pid_t pid;// RECREATES AN EXTRA BECAUSE TOTALLAUNCHED HAS INCREMENTED BUT FUNCTION HASNT TERMINATED 
  
    if (*totalLaunched == simulLimit || totaltoLaunch == 0)
    {
      return (totaltoLaunch);
    }
  else if (totaltoLaunch > 0)
    {
      if ((pid = fork ()) < 0) // FORK HERE
	{
	  perror ("fork");
	}
      else if (pid == 0)
	{
        char* args[]={"./worker",0};
        execlp(args[0],args[0],args[1]);
        
	}
      else if (pid > 0)
	{
  processTable[*totalLaunched].occupied = 1;
  processTable[*totalLaunched].pid = pid;
  processTable[*totalLaunched].startSeconds = *shmSec;
  processTable[*totalLaunched].startNano = *shmNano;
  *totalLaunched += 1;
  srand(time(NULL)*getpid());
  int randSec = rand() % timeLimit + 1;
  int randNano = rand() % 1000000;
  int termTimeSec = randSec + *shmSec;
  int termTimeNano = randNano + *shmNano;
  msg.timeSec = termTimeSec;
  msg.timeNano = termTimeNano;
 if (msgsnd(message_queue,&msg,sizeof(int)*2,0) == -1){
 perror ("MSGSEND");
exit(1);
}
 
	  forker (totaltoLaunch - 1, simulLimit, timeLimit, totalLaunched, processTable,msg,shmSec,shmNano,message_queue);
	}
    }
  else
    return (0);
}

bool checkifChildTerminated(int status, PCB *processTable, int size)
{
    int pid = waitpid(-1, &status, WNOHANG);
    int i = 0;
    if (pid > 0){
    for (i; i < size; i++){
        if (processTable[i].pid == pid)
            processTable[i].occupied = 0;
    }
    return true;
    }
    else if (pid == 0)
        return false;
}
void initializeStruct(struct PCB *processTable){
int i = 0;
for (i; i < 20; i++){
processTable[i].occupied = 0;
processTable[i].pid = 0;
processTable[i].startSeconds = 0;
processTable[i].startNano = 0;
}
}
void printStruct (struct PCB *processTable,int* shmSec,int* shmNano, FILE* fLog)
{
  printf("OSS PID: %d SysClock: %d SysclockNano: %d\n", getpid(),*shmSec,*shmNano);
  printf ("Process Table: \n");
  printf ("ENTRY  OCCUPIED  PID  STARTS  STARTN\n");
  int i = 0;
  for (i; i < 20; i++)
    {
      printf ("%d        %d       %d    %d        %d\n", i,
	      processTable[i].occupied, processTable[i].pid,
	      processTable[i].startSeconds, processTable[i].startNano);
    }
    
    //now write into file
    int j = 0;
  fprintf(fLog,"OSS PID: %d SysClock: %d SysclockNano: %d\n", getpid(),*shmSec,*shmNano);
  fprintf (fLog, "Process Table: \n");
  fprintf (fLog, "ENTRY  OCCUPIED  PID  STARTS  STARTN\n");
    for (j; j < 20; j++)
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
  while ((option = getopt (argc, argv, "n:s:t:f:h")) != -1)
    {
      switch (option)
	{
	case 'h':
	  helpFunction ();
	  return 0;
	case 'n':
	  x = optarg;
	  break;
	case 's':
	  y = optarg;
	  break;
	case 't':
	  z = optarg;
	  break;
	case 'f':
	  LogFile = optarg;
	  break;	    
	}
    }
 
  
    //INITIALIZE ALL VARIABLES
  int totaltoLaunch = 0;	// int to hold -n arg
  int simulLimit = 0;		// int to hold -s arg
  int totalLaunched = 0;	// int to count total children launched
  totaltoLaunch = atoi (x);	// casting the argv to ints
  simulLimit = atoi (y);
  int timeLimit = atoi (z);
  int status; 
  int exCess;
  int *shmSec;
  int *shmNano;
  PCB processTable[20];
  messageQ msg;
  bool initialLaunch = false;
 msg.mtype = 1;
 //create file for LOGFILE
 FILE* fLog;
 fLog = fopen("LogFile", "a");
 if (fLog == NULL)
 {
     printf("ERROR: Could not open file\n");
     return 1;
 }
 
 signal(SIGINT, sig_handler);
 signal(SIGALRM, sig_alarmHandler);
 alarm(60);
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
    
 // initialize struct to 0
 initializeStruct(processTable);
   while(stillChildrenRunning){
  // FORK CHILDREN 
    incrementClock(shmSec,shmNano);
    if (*shmNano == 500000 || *shmNano == 0){
    printStruct (processTable, shmSec, shmNano,fLog);
    }
    if(initialLaunch == false){
        exCess = forker (totaltoLaunch, simulLimit, timeLimit, &totalLaunched, processTable, msg, shmSec, shmNano,message_queue);
        initialLaunch = true;
    }
    bool childHasTerminated = false;
    childHasTerminated = checkifChildTerminated(status, processTable,20);
    if(childHasTerminated == true){
        if (exCess > 0){
            forker (1, 1, timeLimit, &totalLaunched, processTable, msg, shmSec,shmNano,message_queue);
            exCess--;
        }
        totaltoLaunch--;
    }
    if (totaltoLaunch == 0)
            stillChildrenRunning = false;
    }
if (signalReceived == true){ //KILL ALL CHILDREN IF SIGNAL
int i = 0;
pid_t childPid;
for (i;i<20;i++){
	if (processTable[i].pid > 0 && processTable[i].occupied == 1){ // IF there is a process who is still runnning
		childPid = processTable[i].pid;
		kill(childPid, SIGKILL);
	}
}
}

printStruct(processTable, shmSec, shmNano, fLog);
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
