CPPFLAGS=-ggdb
CC=g++

COMMON_OBJECTS=Endpoint.o Semaphore.o Timer.o Protocol.o

all: Client Server

Client: Client.o $(COMMON_OBJECTS)

Server: Server.o $(COMMON_OBJECTS)

Endpoint.o:Endpoint.cpp

Semaphore.o:Semaphore.cpp

Protocol.o:Protocol.cpp

Client.o:Client.cpp

Server.o:Server.cpp

Timer.o:Timer.cpp

Pipe.o:Pipe.cpp
