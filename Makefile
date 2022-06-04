SRC = dataServer.cpp remoteClient.cpp
OBJS =  dataServer.o remoteClient.o utils.o
HEADS = MyQueue.h 
OUTS = dataServer remoteClient

all: $(OUTS)


dataServer:	dataServer.cpp utils.o
	g++ dataServer.cpp  utils.o -o dataServer -lpthread -Wall

remoteClient: remoteClient.cpp utils.o
	g++ remoteClient.cpp  utils.o -o remoteClient -lpthread -Wall

utils.o : utils.cpp
	g++ -c utils.cpp

clean:
	rm -rf output
	rm -f $(OBJS) ${OUTS}


