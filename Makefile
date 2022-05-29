SRC = dataServer.cpp remoteClient.cpp
OBJS =  dataServer.o remoteClient.o
HEADS = MyQueue.h 
OUTS = dataServer remoteClient

all: $(OUTS)


dataServer:	dataServer.cpp
	g++ dataServer.cpp -o dataServer -lpthread -Wall

remoteClient: remoteClient.cpp
	g++ remoteClient.cpp -o remoteClient -lpthread -Wall

clean:
	rm -f $(OBJS) ${OUTS}


