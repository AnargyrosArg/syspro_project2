#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <regex>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "CommonVariables.h"

#define BUFFERSIZE 32

using namespace std;

void createFile(string path , int filesize, int sock);
void createDirectories(string path);
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
    
    string path = "input";

    write(sock,path.c_str(),PATHSIZE);

 
    int nread=0;
    int totalread=0;
    int nread_meta=0;
    int total_meta=0;
    char buf[1];
    char meta[METADATASIZE+1];
    string data;
    string metadata;
    //read metadata for each file
    while((nread_meta=read(sock,meta,METADATASIZE-total_meta))){
        meta[METADATASIZE]='\0';
        metadata.append(meta);
        total_meta = total_meta + nread_meta;
        if(total_meta==METADATASIZE){
            //decode metadata , read exactly size bytes, repeat 
            total_meta=0;
            cout << "Meta:" << metadata<<endl;
            int delim_pos = metadata.rfind(" ");
            int filesize = atoi( metadata.substr(delim_pos+1).c_str());
            string path = metadata.substr(0,delim_pos);
            cout << "filesize "<<filesize<<endl;
            cout << "path " << path <<endl;
            createFile(path,filesize,sock);
            metadata.clear();
            path.clear();
        }        
    }
}



void createFile(string path , int filesize , int sock){
    int file_des;
    path.insert(0,OUTPATH);
    createDirectories(path);
    file_des = open(path.c_str(),O_CREAT | O_RDWR,0777);
    if(file_des==-1){
        perror("open: ");
        exit(-1);
    }
    cout << "Opened: " << path << endl;
    int total_read=0;
    int current_read=0;
    int nread=0;
    
    char buf[1];
    while(total_read<filesize){
        nread=read(sock,buf,1);
        total_read = total_read + nread;
        cout << "Wrote: " << buf[0] << " counter: "<< total_read <<endl;
        write(file_des,buf,1);
    }
}


void createDirectories(string path){
    int start;
    int end=0;

    while((start = path.find_first_not_of('/',end))!= string::npos){
        end = path.find('/',start);
        if(end == string::npos){
            return;
        }
        cout << "trying to create " << path.substr(0,end).c_str() << endl;
        mkdir( path.substr(0,end).c_str() ,0777);
    }


}