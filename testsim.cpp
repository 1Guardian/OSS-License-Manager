/***********************************************************************
testsim code for project 2:

Author: Tyler Martin
Date: 10/6/2021

-Testsim is the binary that actually prints calls the prints to the logfile.
It employs a bakery algorithm to make sure no two instances of itself are
accessing the logfile at the same time. For use with the bakery algorithm
it accesses the shared memory containing the two arrays and it accesses
the shared memory containing the License object and manager

Testing steps:
Program accesses the shared memory areas and gets it's id from the shared
indicies array. It then executes it's own logmsg function which contains
a bakery algorithm inside of it. Once it gains access it logs it's message
and sleeps until it's out of runs.
***********************************************************************/

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <ios>
#include "config.h"

//key for address location
#define SHM_KEY 8273
#define TKTS_ARY 2468
#define CHOS_ARY 1357
#define INDEX_KEY 7911

//Access the shared arrays
int other = shmget(TKTS_ARY, PROCESS_COUNT*sizeof(int), 0600|IPC_CREAT);
int* tickets = (static_cast<int *>(shmat(other, 0, 0)));

int othertwo = shmget(CHOS_ARY, PROCESS_COUNT*sizeof(int), 0600|IPC_CREAT);
int* choosing = (static_cast<int *>(shmat(othertwo, 0, 0)));

//create the shared memory for the indices
int indexmem = shmget(INDEX_KEY, 2*sizeof(int), 0600|IPC_CREAT);
int* indicies = (static_cast<int *>(shmat(indexmem, 0, 0)));

//perror header
std::string exe;

//Shadow of a License Object (needed for casting) 
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
        int getlicense();

        /***********************************************************************
        Function: License->returnlicense()

        Description: Function to increment the number of available
                     licenses by one. 
        ***********************************************************************/
        int returnlicense();

        /***********************************************************************
        Function: License->initlicense()

        Description: Function to initialize the license object 
                     itself, uses the commandline input as a  
                     starting license number.
        ***********************************************************************/
        int initlicense(void);

        /***********************************************************************
        Function: License->addtolicenses(int)

        Description: Function to add 'n' licenses to the pool
                     of available licenses.
        ***********************************************************************/
        void addtolicenses(int n);

        /***********************************************************************
        Function: License->removelicenses(int)

        Description: Function to decrement the number of available
                     licenses by 'n'. 
        ***********************************************************************/
        void removelicenses(int n);

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

int shmid = shmget(SHM_KEY, sizeof(struct License), 0600|IPC_CREAT);
struct License *currentLicenseObject = (static_cast<License *>(shmat(shmid, NULL, 0)));

/***********************************************************************
Function: logmsg(int, int, int)

Description: Function purloined from my own code for project 1.
             Has a default file name to write to, and still sleeps
             between executions, but no longer sleeps for a variable
             or average amount. Once done sleeping, a message is sent
             containing PID, current call and max calls. The reason 
             .append is used so much is because due to library mixing, 
             G++ seems incapable of understanding that "xxx" is an 
             example of an std::string and keeps thinking I want a cstring
             of char[3].

***********************************************************************/
void logmsg(int second, int currentCalls, int maximumCalls, int index){
    if (currentCalls <= maximumCalls){
        //wait designated amount of time
        std::this_thread::sleep_for(std::chrono::seconds(second));
        
        //Bakery Algorithm contained within logmsg
        //var for storing maximum possible element 
        int max_ticket = 0;
        int resource =0;

        // set choosing array element to true
        choosing[index] = 1;
        
        // Finding Maximum ticket value in array
        for (int i = 0; i < PROCESS_COUNT; ++i) {
            int ticket = tickets[i];
            max_ticket = ticket > max_ticket ? ticket : max_ticket;
        }
        // Allotting a new ticket value as MAXIMUM + 1
        tickets[index] = max_ticket + 1;
        
        choosing[index] = 0;
        
        // The ENTRY Section starts from here
        for (int other = 0; other < PROCESS_COUNT; ++other) {
        
            // Applying the bakery algorithm conditions
            while (choosing[other]);
            while (tickets[other] != 0 && (tickets[other] < tickets[index] || (tickets[other] == tickets[index] && other < index)));
        }
            
        //check to see if critical section has been double-accessed
        if (resource != 0) {
            printf("Logfile was granted permission to %d, but is still in-use!\n",
                index);
        }
        
        //set the check lock
        resource = index;

        //get time logged
        time_t curtime;
        time(&curtime);

        //build our message to push
        std::string message="";
        message.append(ctime(&curtime));
        message[message.length()-1] = ' '; //time has a \n for some reason. Removing it.
        message.append("  ").append(std::to_string((int)getpid())).append("   ").append(std::to_string(currentCalls)).append(" of ").append(std::to_string(maximumCalls)).append("\n");
        
        //write message
        currentLicenseObject->logmsg(&message[0]);

        //reset the check lock
        resource = 0;

        //resetting
        tickets[index] = 0;

        //call self again with an increment to the counter
        logmsg(second, currentCalls+1, maximumCalls, index);
    }
}

int main(int argc, char *argv[]){

    //Get the perror header
    std::ifstream("/proc/self/comm") >> exe;
    exe.append(": Error");

    //check all memory allocations
    if(shmid == -1 || indexmem == -1 || other == -1 || othertwo == -1) {
        std::cout <<"Pre-Program Memory allocation error. Program must exit.\n";
        errno = 11;
        perror(&exe[0]);
    }

    //vars
    int sleepTime=0, itterationAmount=0;

    //conversion var
    char* p;

    //Check for sleep time and itteration amount 
    if(argc > 1){
        //check for two additional args
        if(argc > 2){

            //update sleep time
            sleepTime = std::strtol(argv[1], &p, 10);

            //update the itteration amount
            itterationAmount = std::strtol(argv[2], &p, 10);
            
            //index for Bakery
            while(indicies[0]==-1);
            int activeindex = indicies[0];
            indicies[0]=-1;
            shmdt((void *) indicies);

            //start
            logmsg(sleepTime, 1, itterationAmount, activeindex);
        }
        else{
            std::cout << "You must enter both a sleep time and an itteration amount.\n";

            //close arrays
            shmdt((void *) tickets);
            shmdt((void *) choosing);
            exit(1);
        }
    }
    else {
        std::cout << "You must enter a sleep time and an itteration amount.\n";

        //close arrays
        shmdt((void *) tickets);
        shmdt((void *) choosing);
        exit(1);
    }

    //close arrays
    shmdt((void *) currentLicenseObject);
    shmdt((void *) tickets);
    shmdt((void *) choosing);
    return 1;
}