#include "utils.h"


int write_exactly(int sock,std::string data,int bytes_n){
    int nwrite=0;
    int ntotal=0;
    while((nwrite = write(sock,data.c_str()+ntotal,bytes_n-ntotal))>0){
        ntotal  += nwrite;
    }
    return ntotal;
}   