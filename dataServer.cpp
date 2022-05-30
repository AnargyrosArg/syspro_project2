#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <unistd.h>
#include <dirent.h>
#include "MyQueue.h"
#include <set>

#define BUFSIZE 128
using namespace std;

struct Client
{
    int sock;
    pthread_mutex_t client_lock;
};

struct File
{
    string path;
    Client client;
};
void* WorkerThreadFunct(void* arg);
void* CommunicationThreadFunct(void* arg);
vector<string> SearchDirectory(string path);



Queue<File> FileQueue = Queue<File>();
static pthread_mutex_t GeneralQueueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  queue_not_empty;
static pthread_mutex_t queue_not_empty_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  queue_not_full;
static pthread_mutex_t queue_not_full_lock = PTHREAD_MUTEX_INITIALIZER;


//mutex for locking directories, since readdir_r is deprecated and i dont want to use it
//only one thread can search a directory at a time
static pthread_mutex_t dir_read = PTHREAD_MUTEX_INITIALIZER;


int QueueSize;

int main(void){
    int port = 12501;
    int workerCount = 3;
    QueueSize = 10;

    //initialize conditions
    pthread_cond_init(&queue_not_empty,NULL);
    pthread_cond_init(&queue_not_full,NULL);
      //initial lock to not empty mutex so workers pause when created
        if(pthread_mutex_lock(&queue_not_empty_lock)){
            cout << "Error locking queue not empty mutex"<<endl;
            exit(-1);
        }

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
        newClient->sock=newSocket;
        pthread_mutex_init(&(newClient->client_lock),NULL);
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
    pthread_mutex_lock(&client->client_lock);
    char buf[BUFSIZE];
    int n_read;
    //TODO read path    
    while((n_read=read(client->sock,buf,BUFSIZE))>0){
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
    //done using the socket - unlock
    pthread_mutex_unlock(&client->client_lock);
    for(int i=0;i<filenames.size();i++){
        File file;
        file.client=*client;
        file.path=filenames.at(i);

        //for every file read we push in queue and signal not empty
        if(pthread_mutex_lock(&queue_not_empty_lock)){
            cout << "Error locking queue not empty lock in communication thread"<<endl;
            exit(-1);
        }



        //TODO if full -> wait for not full signal
        if(pthread_mutex_lock(&queue_not_full_lock)){
            cout << "Error locking queue not full lock in communication thread"<<endl;
            exit(-1);
        }
        while(FileQueue.size()>=QueueSize){
            pthread_cond_wait(&queue_not_full,&queue_not_full_lock);
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
        
        pthread_mutex_unlock(&queue_not_full_lock);
        pthread_mutex_unlock(&queue_not_empty_lock);


        
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
                filenames.push_back(newpath+"/");
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
         
        while(FileQueue.empty()){
            pthread_cond_wait(&queue_not_empty,&queue_not_empty_lock);
        }
        if(pthread_mutex_lock(&queue_not_full_lock)){
            cout << "Error locking not full queue mutex"<<endl;
            exit(-1);
        }
        if(pthread_mutex_lock(&GeneralQueueLock)){
            cout << "Error locking General queue mutex"<<endl;
            exit(-1);
        }
        File file =  FileQueue.pop();
        cout << "[W-"<<pthread_self()<<"] Working on "<<file.path<<endl;
        pthread_mutex_unlock(&GeneralQueueLock);
        pthread_cond_broadcast(&queue_not_full);
        pthread_mutex_unlock(&queue_not_full_lock);


        pthread_mutex_unlock(&queue_not_empty_lock);
        



        //lock client mutex
        pthread_mutex_lock(&file.client.client_lock);
        //TODO remove sleep
        sleep(1);
        //send file
        //unlock client mutex
        pthread_mutex_unlock(&file.client.client_lock);
    }
}
