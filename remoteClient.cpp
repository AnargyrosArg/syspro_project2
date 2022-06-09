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

#include "utils.h"
#include "CommonVariables.h"

#define BUFFERSIZE 32

using namespace std;

void createFile(string path , int filesize, int sock,int block_size);
void createDirectories(string path);
string sanitizePath(string path);

int main(int argc,char** argv){
    

    hostent *rem;
    string hostname;
    string path ;
    int port = 12501;
    int sock;
    sockaddr_in server;
    sockaddr *serverptr = (sockaddr*)&server;

    bool portUnset = true;
    bool ipUnset = true;
    bool directoryUnset = true;
    if(argc != 7){
        cout << "Usage: ./remoteClient -i <server_ip> -p <server_port> -d <directory>"<<endl;
        exit(-1);
    }
    for(int i=1;i<argc;i=i+2){
        if(!strcmp(argv[i],"-p") && portUnset){
            portUnset = false;
            port = atoi(argv[i+1]);
        }else if(!strcmp(argv[i],"-d") && directoryUnset){
            directoryUnset = false;
            path = argv[i+1];
        }
        else if(!strcmp(argv[i],"-i") && ipUnset){
            ipUnset = false;
            hostname = argv[i+1];
        }
        else{
            cout << "Usage: ./remoteClient -i <server_ip> -p <server_port> -d <directory>"<<endl;
            exit(-1);
        }
    }

    rem = gethostbyname(hostname.c_str());




    if(rem==NULL){
        cout << "gethostbyname: " << hstrerror(h_errno) << endl;
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
    
    
    //maybe while loop here
    write(sock,path.c_str(),PATHSIZE);

 
    int nread_meta=0;
    int total_meta=0;
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
            int block_size = atoi(metadata.substr(delim_pos+1).c_str());
            metadata = metadata.substr(0,delim_pos);
            delim_pos = metadata.rfind(" ");
            int filesize = atoi(metadata.substr(delim_pos+1).c_str());
            string path = metadata.substr(0,delim_pos);
            // cout << "path " << path <<endl;
            // cout << "filesize "<<filesize<<endl;
            // cout << "block_size " << block_size << endl;
            createFile(path,filesize,sock,block_size);
            metadata.clear();
            path.clear();
        }        
    }
}



void createFile(string path , int filesize , int sock,int block_size){
    int file_des;
    cout << path << endl;
    path = sanitizePath(path);
    path.insert(0,OUTPATH);
    createDirectories(path);
    remove(path.c_str());
    file_des = open(path.c_str(),O_CREAT | O_RDWR,0777);
    if(file_des==-1){
        cout << path.c_str() << endl;
        perror("open: ");
        exit(-1);
    }
    cout << "Opened: " << path << endl;
    int total_read=0;
    int block_total=0;
    int nread=0;
    char buf[block_size+1];

    
    while(total_read<filesize){
        cout << path +" "<< total_read << "/"<<filesize << " bytes."<< endl;
        block_total = 0;
        while((nread=read(sock,buf+block_total,min(block_size-block_total,(filesize-total_read))))>0){
            buf[nread]='\0';
            block_total+=nread;
            total_read = total_read + nread;
        }
        write_buffer(file_des,buf,block_total);

    }
    cout << path +" "<< total_read << "/"<<filesize << " bytes."<< endl;
    close(file_des);
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


string sanitizePath(string path){
    int pos ;
    string output = path;
    while( ( pos = output.find("../") !=string::npos)){
        output = output.substr(pos+2);
    }
    while((pos = output.find("./"))!=string::npos){
        output = output.substr(pos+1);
    }
    return output;
}