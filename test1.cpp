//Author: Zihao Zhang
//Date: 5.22.2019

#include "tls.h"
#include <stdlib.h>
#include <stdio.h> 
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <map>
#include <algorithm>
#include <errno.h>

using namespace std;


// used in several tests
//==============================================================================
static void* _thread_dummy(void* arg){
    while(1); //just wait forever
    return 0; //never actually return, just shutting the compiler up
}


int main(){
    pthread_t tid1 = 0;

    // create a thread, to give anyone using their homegrown thread library a chance to init
    pthread_create(&tid1, NULL,  &_thread_dummy, NULL);

    char buf[8192];
    tls_create(8192);
    tls_read(0, 4, buf);
    tls_write(0, 4, buf);
    tls_destroy();

    cout<<"PASS"<<endl;
    return 0;
}