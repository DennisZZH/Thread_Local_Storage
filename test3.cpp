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

// clone test 1
//==============================================================================
static void* _thread_create(void* arg){
    char in_buf[4] = {2,2,2,2};
    sem_t* mutex_sem = (sem_t*)arg;

    tls_create(8192);
    tls_write(0, 4, in_buf);
    sem_post(mutex_sem);

    while(1);
    return 0;
}


int main(){
    pthread_t tid1 = 0;
    sem_t mutex_sem;
    char out_buf[4] = {0};

    // init sem, call thread, and wait until thread 2 creates and writes to it's LSA
    sem_init(&mutex_sem, 0, 0);
    pthread_create(&tid1, NULL,  &_thread_create, &mutex_sem);
    sem_wait(&mutex_sem);

    // clone thread 2's LSA and make sure the values read are what we put there
    tls_clone(tid1);
    tls_read(0, 4, out_buf);

    for(int i = 0; i < 4; i++){
        if(out_buf[i] != 2){
            cout<< "FAIL"<<endl;
        }
    }

    cout<<"PASS"<<endl;

    return 0;
}