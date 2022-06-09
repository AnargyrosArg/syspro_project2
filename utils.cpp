#include "utils.h"
#include <iostream>

int write_exactly(int sock,std::string data,int bytes_n){
    int nwrite=0;
    int ntotal=0;
    while((nwrite = write(sock,data.c_str()+ntotal,bytes_n-ntotal))>0){
        ntotal  += nwrite;
    }
    return ntotal;
}   



int write_buffer(int sock,char* data,int bytes_n){
    int nwrite=0;
    int ntotal=0;
    while((nwrite = write(sock,data + ntotal,bytes_n-ntotal))>0){
        ntotal  += nwrite;
    }
   // std::cout << ntotal <<std::endl;
    return ntotal;
}   