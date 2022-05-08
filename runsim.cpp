/***********************************************************************
runsim code for project 2:

Author: Tyler Martin
Date: 10/6/2021

-Runsim is the 'driver' of this project. It forks off children which in
turn start creating grandchildren via use of "execl", which print messages
to the shared 'License' object. This license object monitors how many
licenses are in use at any given time, and when called, manipulate the
number of licenses in the pool. Runsim will read in commands from testing.data
for the children to execute. It will also spawn two threads: one for monitoring
when a child is done, and adding a license back to the total pool, and a second 
thread for monitoring time since program execution. If the total time spent in 
execution is greter than the time specified in config.h, then the program
terminates immediately.

Testing steps:
Program allocates licenses and license manager, then it creates the shared
arrays for the bakery algorithm and logs start time. It then creates two 
additional threads and begins forking children until a termination 
qualification is achieved, and writes out the termination time to the log.
***********************************************************************/
#include <stdio.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <chrono>
#include <vector>
#include <thread>
#include "string.h"
#include "config.h"

//key for address location
#define SHM_KEY 8273
#define TKTS_ARY 2468
#define CHOS_ARY 1357
#define INDEX_KEY 7911

//global function to set initial license amount
int enteredLicenses=0;

//vector for storing PIDS
std::vector<int> PIDS;
bool amChild = false;

//argv[0] perror header
std::string exe;

//License Object 
struct License
{
    private:
       int nlicenses;

    public:

        /***********************************************************************
        Function: License->printHelp()

        Description: Returns a license if a license is readily
                     available. If not, the parent is left
                     in a spin lock until a license is freed up.
        ***********************************************************************/
        int getlicense(){

            //check if license is available
            if(nlicenses > 0){

                //retrieve license 
                removelicenses(1);

                return 1;
            }
            //license not available 
            else{

                //spinlock
                while(nlicenses < 0);

                //retrieve license 
                removelicenses(1);

                return 1;

            }
        }

        /***********************************************************************
        Function: License->returnlicense()

        Description: Function to increment the number of available
                     licenses by one. 
        ***********************************************************************/
        int returnlicense(){
            nlicenses += 1;
            return 1;
        }

        /***********************************************************************
        Function: License->initlicense()

        Description: Function to initialize the license object 
                     itself, uses the commandline input as a  
                     starting license number.
        ***********************************************************************/
        int initlicense(void){
            //set the maximum allowed number of licenses
            //extern int enteredLicenses;
            nlicenses = enteredLicenses;
            return 1;
        }

        /***********************************************************************
        Function: License->addtolicenses(int)

        Description: Function to add 'n' licenses to the pool
                     of available licenses.
        ***********************************************************************/
        void addtolicenses(int n){
            nlicenses += n;
        }

        /***********************************************************************
        Function: License->removelicenses(int)

        Description: Function to decrement the number of available
                     licenses by 'n'. 
        ***********************************************************************/
        void removelicenses(int n){
            nlicenses -= n;
        }

        /***********************************************************************
        Function: License->logmsg(const char*)

        Description: Function that writes a message to the log file
                     This function treats the logfile as a critical
                     resource. It determines who may log by use of
                     a Bakery algorithm implemetation. It receives
                     the message and the grandchild index in the 
                     char* array 'msg'
        ***********************************************************************/
        void logmsg(const char * msg){
            //write to file
            std::ofstream fout;
            fout.open("logfile", std::ios_base::app);
            fout << msg;
            fout.close();
        }

};

//Make the shared memory for the License object
int shmid = shmget(SHM_KEY, sizeof(struct License), 0600|IPC_CREAT);
struct License *currentLicenseObject = (static_cast<License *>(shmat(shmid, NULL, 0)));

//Make the shared memory for the indicies array
int indexmem = shmget(INDEX_KEY, 2*sizeof(int), 0600|IPC_CREAT);
int* indicies = (static_cast<int *>(shmat(indexmem, 0, 0)));

//make the shared memory for the bakery arrays
int ticketID = shmget(TKTS_ARY, PROCESS_COUNT*sizeof(int), 0600|IPC_CREAT);
int choosingID = shmget(CHOS_ARY, PROCESS_COUNT*sizeof(int), 0600|IPC_CREAT);

/***********************************************************************
Function: docommand(char*)

Description: Function to call the grandchildren 
             (testsim). Uses execl to open a bash
             instance and then simply run the 
             program by providing the name of the bin
             and the relative path appended to the 
             beginning of the command.
***********************************************************************/
void docommand(char* command){

    //add relative path to bin
    std::string relativeCommand = "./";
    relativeCommand.append(command);

    //execute bin
    execl("/bin/bash", "bash", "-c", &relativeCommand[0], NULL);
}

//delete memory funct here
void deleteMemory(){

    //delete each one
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(ticketID, IPC_RMID, NULL);
    shmctl(choosingID, IPC_RMID, NULL);
    shmctl(indexmem, IPC_RMID, NULL);
}

/***********************************************************************
Function: siginthandler(int)

Description: Function that receives control of
             termination when program has received 
             a termination signal
***********************************************************************/
void siginthandler(int param)
{
    printf("\nNow killing all running child processes.\n");
    for(int i=0; i < PIDS.size(); i++)
        kill(PIDS[i], SIGTERM);

    //print the time of shutdown
    time_t curtime;
    time(&curtime);
    std::string shutdownLog("Parent Process Exited at: ");
    shutdownLog.append((ctime(&curtime)));
    currentLicenseObject->logmsg(&shutdownLog[0]);

    //clear memory
    deleteMemory();
    exit(1);
}

/***********************************************************************
Function: threadKill(std::chrono::steady_clock::time_point)

Description: Function that receives control of
             termination when program has received 
             a termination signal from the time 
             contained in config.h being ellapsed.
***********************************************************************/
void threadKill(std::chrono::steady_clock::time_point start)
{
    //don't execute until time has ellapsed.
    while((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < MAX_RUN_TIME) && amChild != true);

    //if child thread, kill self
    if(amChild == true){
        std::cout << "this thread should not exist" << std::endl;
        exit(1);
    }

    //hold licenses so parent can't spawn zombies
    currentLicenseObject->removelicenses(enteredLicenses*3);

    //kill all children
    printf("\nAllotted Execution Time has ellapsed. Now killing all running child processes.\n");
    for(int i=0; i < PIDS.size(); i++)
        kill(PIDS[i], SIGTERM);

    //print the time of shutdown
    time_t curtime;
    time(&curtime);
    std::string shutdownLog("Parent Process Exited at: ");
    shutdownLog.append((ctime(&curtime)));
    currentLicenseObject->logmsg(&shutdownLog[0]);

    //clear memory 
    deleteMemory();
    exit(1);
}

/***********************************************************************
Function: threadReturn()

Description: Function that a thread will inhabit with
             the sole purpose and intent of keeping 
             track of which child processes are still
             running and will keep the licenses in 
             check by returning them.
***********************************************************************/
void threadReturn()
{
    while(1){
        if(waitpid(-1, NULL, WNOHANG) > 0)
            currentLicenseObject->returnlicense();
    }
}

/***********************************************************************
Function: createBakeryArrays()

Description: Function that creates the shared arrays 
             that will be used in the Bakery algorithm
             returns -1 on failure of allocation.
             returns 0 on successful allocation.
***********************************************************************/
int createBakeryArrays(){

    //check that the array space is valid
    if (ticketID == -1) {
        errno = 11;
        perror(&exe[0]);
        return -1;
    }
    //if all is well, update
    int* tickets = (static_cast<int *>(shmat(ticketID, 0, 0)));

    //chec kthat the array space is valid
    if (choosingID == -1) {
        errno = 11;
        perror(&exe[0]);
        return -1;
    }
    //if all is well, update
    int* choosing = (static_cast<int *>(shmat(choosingID, 0, 0)));

    //fill arrays
    memset((void*)tickets, 0, sizeof(tickets));
    memset((void*)choosing, 0, sizeof(choosing));

    //disconnect from arrays
    shmdt((void *) tickets);
    shmdt((void *) choosing);

    return 0;
}

/***********************************************************************
Function: childProcess(std::string, pid_t, int)

Description: Function that executes docommand and 
             waits for child process to finish.
***********************************************************************/
void childProcess(std::string currentCommand, pid_t wpid, int status){
    //child code
    amChild = true;

    //run docommand
    std::cout << "Child Starting:" << std::endl;
    docommand(&currentCommand[0]);
            
    //spin-locking the children
    while ((wpid = wait(&status)) > 0);
}

/***********************************************************************
Function: parentProcess(int)

Description: Function that acts as the parent function.
             Swaps to child function upon determining
             that it is a child.
***********************************************************************/
void parentProcess(int counter){
    //get license as the parent to start the child
    currentLicenseObject->getlicense();

    //keep track of the indicies of the grandchildren
    //spinlock to ensure the indicies are assigned properly
    while(indicies[0]!=-1);

    //update the value for the index
    indicies[0] = counter;
}

/***********************************************************************
Function: main(int, char*)

Description: Main function. Starts by initializing the 
             license number contained within the license
             object. It then allocates shared memory
             and begins to read in from stdin. It
             forks children as long as there are 
             free licenses to be had. Otherwise, it
             enters a spinlock until one is freed up.
***********************************************************************/
int main(int argc, char *argv[])
{
    
    //Get the perror header
    std::ifstream("/proc/self/comm") >> exe;
    exe.append(": Error");

    //capture sigint 
    signal(SIGINT, siginthandler);
    
    //pointer for conversion
    char* p; 

    //checking if a file was provided
    if (isatty(fileno(stdin))) {
        errno = 22;
        std::cerr << &exe[0] << "Usage: ./runsim n < inputfile " << std::endl;
        perror(&exe[0]);
        exit(EXIT_FAILURE);
    }

    //stream holders
    std::string currentCommand;

    //forking variables
    pid_t child_pid, wpid;
    int status = 0;
    int n=5;
    int counter = 0;

    //Check for Number of licenses to allow 
    if(argc > 1){

        enteredLicenses = std::strtol(argv[1], &p, 10)-1;

        if(enteredLicenses < 0 || enteredLicenses > 20){
            std::cout << "The number of allowed processes must be larger than 0 and less than 20.\n";
            exit(EXIT_FAILURE);
        }
        else if(shmid == -1 || indexmem == -1) {
            std::cout << "Pre-Program Memory allocation error. Program must exit.\n";
            errno = 11;
            perror(&exe[0]);
            exit(EXIT_FAILURE);
        }

        //setup the bakery arrays
        if (createBakeryArrays() == -1) {
            errno = 4;
            perror(&exe[0]);
            exit(1);
        }

        //update the number of licenses
        currentLicenseObject->initlicense();

    }
    else {
        std::cout << "You must enter a number of programs to run.\n";
        exit(1);
    }

    //parent code
    std::cout << "Parent Starting:" << std::endl;

    //init indicies for first run
    indicies[0] = -1;

    //make our time-watching thread and logging start-time
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::thread timeWatcher (threadKill, start);

    //second thread for checking for returned children
    std::thread childrenWatcher (threadReturn);

    //Read in the commands
    while (std::getline(std::cin, currentCommand)){

        //core of the parent process
        parentProcess(counter);

        //check to see if process is child or parent
        if ((child_pid = fork()) == 0) {
            
            //start child process
            childProcess(currentCommand, wpid, status);

            //close index access
            shmdt((void *) indicies);
            exit(0);
        }

        //Adds the pid to the vector
        PIDS.push_back((int)child_pid);

        //update the counter
        counter += 1;
    }

    //spin-locking the parent
    while ((wpid = wait(&status)) > 0);
    std::cout << "Parent Ending:" << std::endl;
    //Father code (After all child processes end)
    shmdt((void *) indicies);
    
    //delete the shared memory
    deleteMemory();

    //print the time of shutdown
    time_t curtime;
    time(&curtime);
    std::string shutdownLog("Parent Process Exited at: ");
    shutdownLog.append((ctime(&curtime)));
    currentLicenseObject->logmsg(&shutdownLog[0]);

    return 0;
}
