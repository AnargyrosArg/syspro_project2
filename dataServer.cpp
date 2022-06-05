#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "MyQueue.h"
#include <fcntl.h>
#include <stdlib.h>
#include <set>


#include "utils.h"
#include "CommonVariables.h"
//maximum length of initial path size the client can request

using namespace std;




struct Client
{
    int sock;
    pthread_mutex_t* client_lock;
    int remaining_files;
};

struct File
{
    string path;
    Client* client;
};


void* WorkerThreadFunct(void* arg);
void* CommunicationThreadFunct(void* arg);
vector<string> SearchDirectory(string path);
void sendFile(string path,int socket);


Queue<File> FileQueue = Queue<File>();
static pthread_mutex_t GeneralQueueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  queue_not_empty;
static pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  queue_not_full;


//mutex for locking directories, since readdir_r is deprecated and i dont want to use it
//only one thread can search a directory at a time
static pthread_mutex_t dir_read = PTHREAD_MUTEX_INITIALIZER;


int QueueSize;
int block_size;


int main(int argc , char** argv){
    if(argc!=9){
        cout << "Usage: ./dataServer -p <port> -s <thread_pool_size> -q <queue_size> -b <block_size>"<<endl;
        exit(-1);
    }
    int port = 12501;
    int workerCount = 5;
    QueueSize = 10;
    block_size = 16;
    for(int i=1;i<argc;i=i+2){
        if(!strcmp(argv[i],"-p")){
            port = atoi(argv[i+1]);
        }else if(!strcmp(argv[i],"-s")){
            workerCount = atoi(argv[i+1]);
        }
        else if(!strcmp(argv[i],"-q")){
            QueueSize = atoi(argv[i+1]);
        }
        else if(!strcmp(argv[i],"-b")){
            block_size = atoi(argv[i+1]);
        }else{
            cout << "Usage: ./dataServer -p <port> -s <thread_pool_size> -q <queue_size> -b <block_size>"<<endl;
            exit(-1);
        }
    }




    //initialize conditions
    pthread_cond_init(&queue_not_empty,NULL);
    pthread_cond_init(&queue_not_full,NULL);
   
   
    //create worker threads
    pthread_t workers[workerCount];
    for(int i=0;i<workerCount;i++){
        workers[i] = pthread_create(&workers[i],NULL,WorkerThreadFunct,NULL);
    }
    

    //server socket stuff , partly (pretty much all) taken directly from class notes
    int sock;
    struct sockaddr_in server;
    struct sockaddr * serverptr =( struct sockaddr *) &server;

    if((sock=socket(AF_INET,SOCK_STREAM, 0 ))<0){
        perror("socket:");
        exit(-1);
    }
    server.sin_family =  AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if(bind(sock , serverptr , sizeof(server))<0){
        perror("bind:");
        exit(-1);
    }
    if(listen(sock,10)<0){
        perror("listen:");
        exit(-1);
    }
    //infinite loop accepting connections
    while(true){
        int newSocket;
        //block until we recieve a new connection
        if((newSocket=accept(sock, NULL , 0))<0){
            perror("accept:");
            exit(-1);
        } 
        //connection recived
        //build a new client
        pthread_t communication_thread;
        Client *newClient = new Client;
        newClient->client_lock =(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        newClient->sock=newSocket;
        pthread_mutex_init(newClient->client_lock,NULL);
        //create a new communication thread
        if(pthread_create(&communication_thread,NULL,CommunicationThreadFunct,newClient)){
            cout << "Error creating new communication thread."<<endl;
            exit(-1);
        }
        //since all the this thread does is scan for files to be copied over
        //we dont need to wait it so we detach it immediatelly
        if(pthread_detach(communication_thread)){
            cout << "Error detaching communication thread"<<endl;
            exit(-1);
        }
    }
}


void* CommunicationThreadFunct(void* arg){
    Client* client = (Client*) arg;
    vector<string> filenames;
    string path = "";
    //exclusive use of client socket
    pthread_mutex_lock(client->client_lock);
    char buf[PATHSIZE];
    int n_read;
    //TODO read path    
    int total_read=0;
    while((n_read=read(client->sock,buf,PATHSIZE-total_read))>0){
        total_read = total_read + n_read;
        path.append(buf);
    }
    if(path.at(path.size()-1)=='/'){
        path.erase(path.size()-1);
    }
    // SearchDirectory uses readdir which is not thread safe
    // the thread safe version is deprecated and i dont want to use it
    // so i decided to externally lock directory reading with a mutex
    pthread_mutex_lock(&dir_read);
    filenames = SearchDirectory(path);
    pthread_mutex_unlock(&dir_read);

    client->remaining_files = filenames.size();
    
    
    //done using the socket - unlock
    pthread_mutex_unlock(client->client_lock);
    for(int i=0;i<filenames.size();i++){
        File file;
        file.client=client;
        file.path=filenames.at(i);

        //for every file read we push in queue and signal not empty
        if(pthread_mutex_lock(&cond_mutex)){
            cout << "Error locking queue not empty lock in communication thread"<<endl;
            exit(-1);
        }

        while(FileQueue.size()>=QueueSize){
            pthread_cond_wait(&queue_not_full,&cond_mutex);
        }
        
        
        //generic lock here to avoid race condition on the Queue
         if(pthread_mutex_lock(&GeneralQueueLock)){
            cout << "Error locking general queue lock in communication thread"<<endl;
            exit(-1);
        }
        FileQueue.push(file);
        cout <<"[C-"<<pthread_self()<<"] Added "<<filenames.at(i)<<" to the queue."<<endl;
        cout << "Current queue size: "<< FileQueue.size() << endl;
        pthread_mutex_unlock(&GeneralQueueLock);    
        
        pthread_cond_broadcast(&queue_not_empty);
        pthread_mutex_unlock(&cond_mutex);


        
    }
    pthread_exit(0);
    return NULL;
}


vector<string> SearchDirectory(string path){
    DIR *dir;
    dirent * contents;
    vector<string> filenames = vector<string>();

    //open directory
    if((dir = opendir(path.c_str()))==NULL){
        cout << "Couldn't open given directory: " << path <<endl;

        pthread_exit(0);
    }  
    //read everything
    while((contents = readdir(dir))!=NULL){
        string filename = contents->d_name;
        string newpath = path;
        //if content is not the directory itself or its parent
        if(filename.compare(".")!=0 && filename.compare("..")!=0 ){
            //if content is itself a directory call this function for it
            if(contents->d_type == DT_DIR){
                newpath.append("/"+filename);
                vector<string> temp = SearchDirectory(newpath);
              //  filenames.push_back(newpath+"/");
                filenames.insert(filenames.end() , temp.begin(),temp.end()); 
            }else{
            //else just store the name of the file
                filenames.push_back(path+"/"+filename);
            }
        }
    }
    closedir(dir);
    return filenames;
}


void* WorkerThreadFunct(void* arg){
    while(true){
        
        if(pthread_mutex_lock(&cond_mutex)){
            cout << "Error locking not cond mutex"<<endl;
            exit(-1);
        }


        while(FileQueue.empty()){
            pthread_cond_wait(&queue_not_empty,&cond_mutex);
        }
        if(pthread_mutex_lock(&GeneralQueueLock)){
            cout << "Error locking General queue mutex"<<endl;
            exit(-1);
        }
        File file =  FileQueue.pop();
        cout << "[W-"<<pthread_self()<<"] Working on "<<file.path<<endl;
        cout << "Current size: " << FileQueue.size() << endl;
        pthread_mutex_unlock(&GeneralQueueLock);
        pthread_cond_broadcast(&queue_not_full);

        pthread_mutex_unlock(&cond_mutex);
        



        //lock client mutex
        pthread_mutex_lock(file.client->client_lock);

        //send file
        sendFile(file.path,file.client->sock);
        //if the client has recieved all files requested 
        //we can close the socket
        file.client->remaining_files--;
        if(file.client->remaining_files==0){
            cout << "Client in socket "<< file.client->sock << " done!"<<endl;
            close(file.client->sock);
        }
        //unlock client mutex
        pthread_mutex_unlock(file.client->client_lock);
    }
}




void sendFile(string path,int socket){
    int file_desc;
    int file_size;

    char buf[block_size];
    file_desc = open(path.c_str(),O_RDONLY);
    if(file_desc==-1){
        perror("fopen: ");
        exit(-1);
    }
    file_size = lseek(file_desc,SEEK_SET,SEEK_END);
    lseek(file_desc,SEEK_SET,0);
    
    int total = 0;
    int nread = 0;
    int nwrite;
    int blocktotal = 0;
    string data ="";
    char metadata[METADATASIZE];
    strcpy(metadata,(path+" "+to_string(file_size)).c_str());
    
    //maybe while loop here
    cout << "wrote: " << write(socket,metadata,METADATASIZE)<< "should have been " << METADATASIZE<<endl;


    while(total<file_size){
        while((nread=read(file_desc,buf,block_size-blocktotal))>0){
            total = total + nread;
            blocktotal= blocktotal + nread;
            data.append(buf);
        }
        //while loop here
        nwrite=write(socket,data.c_str(),blocktotal);
        cout << data.c_str() << endl;
        data.clear();
        blocktotal = 0;            
    }
    
    close(file_desc);
}   