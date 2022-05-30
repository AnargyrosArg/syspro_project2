#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


using namespace std;


int main(void){
    

    hostent *rem;

    int port = 12501;
    int sock;
    sockaddr_in server;
    sockaddr *serverptr = (sockaddr*)&server;

    if((rem = gethostbyname("localhost"))==NULL){
        perror("gethostbyname: ");
        exit(-1);
    }        

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr , rem->h_addr , rem->h_length);
    server.sin_port = htons(port);
    if((sock=socket(AF_INET,SOCK_STREAM, 0 ))<0){
        perror("socket:");
        exit(-1);
    }

    if(connect(sock , serverptr ,  sizeof(server))<0){
        perror("connect: ");
        exit(-1);
    }
    char test[] ="output";
    write(sock,test,sizeof(test));
    cout << "wrote " << test << " in socket "<<endl;
}