#Vars
CC=g++
CFLAGS=-std=c++11 -pthread
OBJECTS=testsim runsim
DEPS=config.h

#Make both bin objects
all: $(OBJECTS)

testsim:
	$(CC) $(CFLAGS) -o testsim testsim.cpp $(DEPS)

runsim:
	$(CC) $(CFLAGS) -o runsim runsim.cpp $(DEPS)

#Clean
clean: 
	rm $(OBJECTS) 