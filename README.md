# OSS-License-Manager

## Notes

- The driver program takes in a number of concurrent processes to allow and 
forks off child processes which in turn execls a grandchild. Each time the
parent calls for a new child, it requests a license object from the manager.
The parent spinlocks until the license manager has an available license to give
and then begins the fork.

-There is another thread that gets spawned from the Parent upon initialization. 
The thread manages the timer for the termination of the program. The time that the 
program runs and the thread checks is defined with in config.h. When the thread reaches
the termination time, it begins the shutdown process by hogging all the licenses to prevent
any zombie processes, and then it initiates the exit. 

-There is another implicit handler for looking for the SIGINT signal, and it invokes the same
shutdown process that the time management thread invokes. 

## Logger Library and Driver: Overview

The runsim program is the parent process for this entire project. It acts as the interface
between obtaining licenses and creating children (testsim). It also acts as the spawnpoint for
the time management thread which keeps track of whether or not termination time has been reached.

The testsim program acts as the child, and interfaces with all the active testsim programs to 
print out messages into a log file through the use of the bakery algorithm. Once this program is 
excel'd into existence from the child process of runsim. 

./runsim [number of processes] < testing.data

## Getting Started (Compiling)

To compile both the runsim and testsim programs, simple use of 'make'
will compile both for your use. 'make clean' will remove the created object files and the
executable binary that was compiled using the object files
