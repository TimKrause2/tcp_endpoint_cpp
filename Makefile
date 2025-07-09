CPPFLAGS=-ggdb
CC=g++

COMMON_OBJECTS=Endpoint.o Fifo.o Semaphore.o Timer.o Protocol.o \
    RecvFifo.o

all: Client Server

Client: Client.o $(COMMON_OBJECTS)

Server: Server.o $(COMMON_OBJECTS)

Endpoint.o:Endpoint.cpp

Fifo.o:Fifo.cpp

Semaphore.o:Semaphore.cpp

Protocol.o:Protocol.cpp

Client.o:Client.cpp

Server.o:Server.cpp

Timer.o:Timer.cpp

Pipe.o:Pipe.cpp

RecvFifo.o:RecvFifo.cpp
