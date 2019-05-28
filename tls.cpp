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

static unsigned int PAGESIZE;
static int numOfTLS = 0;
static map<pthread_t, TLS> tls_map;


bool scan_TLS_find_page_fault(unsigned int page_fault){
    for(auto it1 = tls_map.begin(); it1 != tls_map.end(); it1++){
        for(auto it2 = it1->second.pages.begin(); it2 != it1->second.pages.end(); it2++){
            if(it2->page_address == page_fault){
                return true;
            }
        }
    }
    return false;
}


void page_fault_handler(int sig, siginfo_t *si, void *context){
    unsigned int page_fault = (unsigned int) si->si_addr & ~(PAGESIZE - 1);
    if(scan_TLS_find_page_fault(page_fault) == true){
        pthread_exit(NULL);
    }else{
        signal(SIGSEGV, SIG_DFL); 
        signal(SIGBUS, SIG_DFL); 
        raise(sig);
    }

}


void tls_init(){
    struct sigaction sigact;
    PAGESIZE = getpagesize();
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = page_fault_handler;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);
}


int tls_create(unsigned int size){

    if(numOfTLS == 0){
        tls_init();
    }

    if(size <= 0){
        return -1;
    }

    pthread_t thread_id = pthread_self();
    if(tls_map.find(thread_id) != tls_map.end()){
        return -1;
    }

    TLS new_tls;
    new_tls.tls_id = thread_id;
    new_tls.tls_size = size;
    new_tls.page_num = (size -1)/PAGESIZE + 1;

    for (int cnt = 0; cnt < new_tls.page_num; cnt++) { 
        Page *new_page;
        new_page = (Page *) calloc(1, sizeof(Page));
        new_page->page_address = (unsigned int) mmap(NULL, PAGESIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        new_page->reference_count = 1;
        new_tls.pages[cnt] = new_page;
    }

    tls_map.insert(pair<pthread_t, TLS>(thread_id, new_tls));

    return 0;   // If success
}


int tls_destroy(){

    pthread_t thread_id = pthread_self();
    auto it = tls_map.find(thread_id);
    if(it == tls_map.end()){
        return -1;
    }

    for(int i = 0; i < it->second.page_num; i++){
        if(it->second.pages[i]->reference_count > 1){
            it->second.pages[i]->reference_count--;
        }else{
            munmap(it->second.pages[i]->page_address, PAGESIZE);
            free(it->second.pages[i]);
        }
    }
    tls_map.erase(it);

    return 0;
}


void tls_protect(Page *p) {
    if (mprotect((void *) p->page_address,PAGESIZE, 0)){ 
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    } 
}


void tls_unprotect(Page *p) {
    if (mprotect((void *) p->page_address,PAGESIZE, PROT_READ | PROT_WRITE)){
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1); 
    }
}


int tls_read(unsigned int offset, unsigned int length, char *buffer){

    pthread_t thread_id = pthread_self();
    auto it = tls_map.find(thread_id);
    if(it == tls_map.end()){
        return -1;
    }

    if(offset + length > it->second.tls_size){
        return -1;
    }

    for(int i = 0; i < it->second.page_num; i++){
        tls_unprotect(it->second.pages[i]);
    }

    /* perform the read operation */
    for (int cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
        Page *p;
        unsigned int pn, poff;
        pn = idx / PAGESIZE;
        poff = idx % PAGESIZE;
        p = it->second.pages[pn];
        char* src = ((char *) p->page_address) + poff;
        buffer[cnt] = *src;
    }

    for(int i = 0; i < it->second.page_num; i++){
        tls_protect(it->second.pages[i]);
    }
    
    return 0;
}


int tls_write(unsigned int offset, unsigned int length, char *buffer){

    pthread_t thread_id = pthread_self();
    auto it = tls_map.find(thread_id);
    if(it == tls_map.end()){
        return -1;
    }

    if(offset + length > it->second.tls_size){
        return -1;
    }

    for(int i = 0; i < it->second.page_num; i++){
        tls_unprotect(it->second.pages[i]);
    }

    /* perform the write operation */
    for (int cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
        Page *p, *copy; 
        unsigned int pn, poff; 
        pn = idx / PAGESIZE; 
        poff = idx % PAGESIZE; 
        p = it->second.pages[pn];

        if (p->reference_count > 1) {
            // this page is shared, create a private copy (COW) */ 
            //Do copy on Write here
            copy = (Page *) calloc(1, sizeof(Page));
            copy->page_address = (unsigned int) mmap(NULL, PAGESIZE, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
            //printf("[+] allocated %x\n", copy->page_address); 
            copy->reference_count = 1;
            it->second.pages[pn] = copy;
            /* update original page */
            p->reference_count--; 
            tls_protect(p);
            p = copy;
        }
        char* dst = ((char *) p->page_address) + poff; 
        *dst = buffer[cnt];
    }
            
    for(int i = 0; i < it->second.page_num; i++){
        tls_protect(it->second.pages[i]);
    }

    return 0;
}


int tls_clone(pthread_t tid){
    
    pthread_t thread_id = pthread_self();
    auto it1 = tls_map.find(thread_id);
    if(it1 != tls_map.end()){               // The copying thread already has TLS
        return -1;
    }

    auto it2 = tls_map.find(tid);
    if(it2 == tls_map.end()){               // The thread being copied has no TLS
        return -1;
    }

    TLS new_tls;
    new_tls.tls_id = thread_id;
    new_tls.tls_size = it2->second.tls_size;
    new_tls.page_num = it2->second.page_num;

    for (int cnt = 0; cnt < new_tls.page_num; cnt++) { 
        Page *new_page;
        new_page = it2->second.pages[cnt];
        new_page->reference_count++;
        new_tls.pages[cnt] = new_page;
    }

    tls_map.insert(pair<pthread_t, TLS>(thread_id, new_tls));

    return 0;

}


void* tls_get_internal_start_address(){

    pthread_t thread_id = pthread_self();
    auto it = tls_map.find(thread_id);
    if(it == tls_map.end()){               // The current thread has no TLS
        return NULL;
    }

    return (void*) it->second.pages[0]->page_address;
}
