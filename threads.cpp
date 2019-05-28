//Author: Zihao Zhang
//Date: 4.21.2019

#include "threads.h"
#include <stdlib.h>
#include <stdio.h> 
#include <iostream>
#include <sys/time.h>
#include <signal.h>
#include <list>
#include <algorithm>
#include <errno.h>

using namespace std;

#define INTERVAL 50				/* time interval in milliseconds */
#define MAX 128					/* max number of threads allowed */
#define MAIN_ID 0
#define SEM_VALUE_MAX 65536		/* max semaphore value */

static sigset_t mask;			// blocking list
static int numOfThreads = 0;
static int numOfSemaphore = 0;
static pthread_t curr_thread_id = 0;
static list<TCB> thread_pool;
static list<Semaphore> semaphore_list;


/* Helper Functions*/
void print_thread_pool(){
	printf("curr thread = %d \n", curr_thread_id);
    printf("size = %d \n", thread_pool.size());
    list<TCB>::iterator it;
	for (it = thread_pool.begin(); it != thread_pool.end(); ++it){
    	cout << it->thread_id<<" "<<it->thread_state<<endl;
	}
	cout<<endl;
}


TCB* find_thread_by_id(pthread_t id){
	list<TCB>::iterator it;
	for (it = thread_pool.begin(); it != thread_pool.end(); ++it){
    	if(it->thread_id == id){
			return &*it;										// return a pointer to the target thread
		}
	}
	return NULL;												//Return NULL if not found
}

void delete_thread_by_id(pthread_t id){
	while(thread_pool.front().thread_id != id){					// find the target thread
		thread_pool.push_back(thread_pool.front());
        thread_pool.pop_front();
	}
	thread_pool.pop_front();									// delete target thread
	while(thread_pool.front().thread_id != curr_thread_id){		// roll thread pool back in order
		thread_pool.push_back(thread_pool.front());
        thread_pool.pop_front();
	}
}


void find_next_active_thread(){									// find the next active thread, used by scheduler
	while(thread_pool.front().thread_state != TH_ACTIVE){		// the active thread is end up at the front of the list
		thread_pool.push_back(thread_pool.front());
        thread_pool.pop_front();
	}
}


void free_all_threads(){
     while(thread_pool.empty() == false){
		 if(thread_pool.front().thread_id == MAIN_ID){
			 thread_pool.pop_front();
		 }else{
			free( thread_pool.front().thread_free);
			thread_pool.pop_front();
		 }
		 numOfThreads--;
     }
 }


Semaphore* find_semaphore_by_id(int id){
	list<Semaphore>::iterator it;
	for (it = semaphore_list.begin(); it != semaphore_list.end(); ++it){
    	if(it->semaphore_id == id){
			return &*it;
		}
	}
	return NULL;												//Return NULL if not found
}

void delete_semaphore_by_id(int id){
	while(semaphore_list.front().semaphore_id != id){
		semaphore_list.push_back(semaphore_list.front());
        semaphore_list.pop_front();
	}
	semaphore_list.pop_front();
}


// mangle function
static long int i64_ptr_mangle(long int p){
    long int ret;
    asm(" mov %1, %%rax;\n"
        " xor %%fs:0x30, %%rax;"
        " rol $0x11, %%rax;"
        " mov %%rax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%rax"
        );
        return ret;
}


void wrapper_function(){
	
	unlock();

	TCB executing_thread = thread_pool.front();

    void* return_value = executing_thread.thread_start_routine(thread_pool.front().thread_arg);

    pthread_exit(return_value);

}


void thread_schedule(int signo){

  if(thread_pool.size() <= 1){
      return;
  }
  
  lock();
  if(setjmp(thread_pool.front().thread_buffer) == 0){

		thread_pool.push_back(thread_pool.front());

        thread_pool.pop_front();

		find_next_active_thread();

        curr_thread_id = thread_pool.front().thread_id;

		longjmp(thread_pool.front().thread_buffer,1);

	}

	unlock();
    return;
}


// one time set up alarm
void setup_timer_and_alarm(){
	
	struct itimerval it_val;
	struct sigaction siga;

	siga.sa_handler = thread_schedule;
	siga.sa_flags =  SA_NODEFER;

 	if (sigaction(SIGALRM, &siga, NULL) == -1) {
    	perror("Error calling sigaction() !\n");
    	exit(1);
 	}
  
  	it_val.it_value.tv_sec =     INTERVAL/1000;
  	it_val.it_value.tv_usec =    (INTERVAL*1000) % 1000000;   
  	it_val.it_interval = it_val.it_value;
  
  	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
    	perror("Error calling setitimer() !\n");
    	exit(1);
 	}

}


// block alarm, disable interuption
void lock(){
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	if( sigprocmask(SIG_BLOCK, &mask, NULL) < 0 ){
		perror("Error locking !\n");
		exit(1);
	}
}


// resume interuption
void unlock(){
	if( sigprocmask(SIG_UNBLOCK, &mask, NULL) < 0 ){
		perror("Error unlocking !\n");
		exit(1);
	}
}


/*Thread Library*/
void pthread_init(){										//Call to initialize thread system, then add main thread to thread table

	TCB main_thread;
	main_thread.thread_id = (pthread_t) numOfThreads;		// Main thread's id is 0
	main_thread.thread_state = TH_ACTIVE;

	main_thread.thread_start_routine = NULL;
	main_thread.thread_arg = NULL;
	main_thread.thread_free = NULL;
	main_thread.exit_code = NULL;
	main_thread.joinfrom_thread = -1;
	
	setjmp(main_thread.thread_buffer);
	
	thread_pool.push_back(main_thread);
    numOfThreads++;

	setup_timer_and_alarm();

}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg){
	
	// Prepare thread system
	if(numOfThreads == 0){
		pthread_init();
	}

	// Create new thread
	lock();

	TCB new_thread;
	new_thread.thread_id = (pthread_t) numOfThreads;
	new_thread.thread_state = TH_ACTIVE;

	new_thread.thread_start_routine = start_routine;
	new_thread.thread_arg = arg;

	new_thread.exit_code = NULL;
	new_thread.joinfrom_thread = -1;

	setjmp(new_thread.thread_buffer);
	
	unsigned long *new_sp = (unsigned long*) malloc(32767);
	new_thread.thread_free = new_sp;

	void (*wrapper_function_ptr)() = &wrapper_function;
	
	new_thread.thread_buffer[0].__jmpbuf[6] = i64_ptr_mangle((unsigned long)(new_sp + 32767 / 8 - 2));

	new_thread.thread_buffer[0].__jmpbuf[7] = i64_ptr_mangle((unsigned long)wrapper_function_ptr);

	*thread = new_thread.thread_id;

	thread_pool.push_back(new_thread);
    numOfThreads++;

	unlock();

	return 0;	// If success

}


void pthread_exit(void *value_ptr){
	
	if(curr_thread_id == MAIN_ID){								// main thread exit, clean up memory, terminate the process
		
		lock();
		free_all_threads();
		unlock();

		exit(0);

	}else{														// regular thread exit

		lock();

		thread_pool.front().thread_state = TH_DEAD;
		thread_pool.front().exit_code = value_ptr;

		if(thread_pool.front().joinfrom_thread != -1){			// If it is joint from somewhere

			TCB* join_thread = find_thread_by_id(thread_pool.front().joinfrom_thread);
			join_thread->thread_state = TH_ACTIVE;

		}

		unlock();

		thread_schedule(1);
        
	}
}


pthread_t pthread_self(void){
	return curr_thread_id;
}


int pthread_join(pthread_t thread, void **value_ptr){

	lock();
	TCB* target_thread = find_thread_by_id(thread);

	if(target_thread == NULL){
		cerr<<"Error finding target thread !"<<endl;
		errno = ESRCH;
		unlock();
		return errno;
	}

	if(target_thread->joinfrom_thread != -1){
		cerr<<"Error other thread is waiting for this thread !"<<endl;
		errno = EINVAL;
		unlock();
		return errno;
	}

	if(thread_pool.front().joinfrom_thread == thread){
		cerr<<"Error deadlock detected !"<<endl;
		errno = EDEADLK;
		unlock();
		return errno;
	}

	if(target_thread->thread_state == TH_ACTIVE || target_thread->thread_state == TH_BLOCKED){		// block and wait

		thread_pool.front().thread_state = TH_BLOCKED;
		target_thread->joinfrom_thread = curr_thread_id;
		unlock();
	
		thread_schedule(1);

	}else{
		unlock();
	}

	lock();

	target_thread = find_thread_by_id(thread);

	if(value_ptr != NULL){
		*value_ptr = target_thread->exit_code;
	}

	free(target_thread->thread_free);
	delete_thread_by_id(target_thread->thread_id);
	numOfThreads--;		

	unlock();
	
	return 0;	//If success
}


int sem_init(sem_t *sem, int pshared, unsigned int value){

	lock();
	Semaphore new_semaphore;
	new_semaphore.semaphore_id = numOfSemaphore;
	new_semaphore.semaphore_value = value;

	semaphore_list.push_back(new_semaphore);
	numOfSemaphore++;

	sem->__align = new_semaphore.semaphore_id;
	unlock();

	return 0;	// If success
}


int sem_destroy(sem_t *sem){

	lock();
	int target_id = sem->__align;

	Semaphore* target_semaphore = find_semaphore_by_id(target_id);
	if(target_semaphore == NULL){
		perror("Error finding target semaphore !\n");
		unlock();
		return -1;
	}

	if(target_semaphore->waiting_list.empty() != true){
		perror("Error destroying target semaphore : thread waiting list is not empty !\n");
		unlock();
		return -1;
	}

	delete_semaphore_by_id(target_id);
	numOfSemaphore--;
	unlock();

	return 0;	// If success
}


int sem_wait(sem_t *sem){

	lock();
	int target_id = sem->__align;

	Semaphore* target_semaphore = find_semaphore_by_id(target_id);
	if(target_semaphore == NULL){
		perror("Error finding target semaphore !\n");
		unlock();
		return -1;
	}

	if(target_semaphore->semaphore_value > 0){

		target_semaphore->semaphore_value --;
		unlock();

	}else{

		thread_pool.front().thread_state = TH_BLOCKED;
		
		target_semaphore->waiting_list.push(thread_pool.front().thread_id);
		unlock();

		thread_schedule(1);

	}
	
	return 0;	// If success

}


int sem_post(sem_t *sem){

	lock();
	int target_id = sem->__align;

	Semaphore* target_semaphore = find_semaphore_by_id(target_id);
	if(target_semaphore == NULL){
		perror("Error finding target semaphore !\n");
		unlock();
		return -1;
	}

	if(target_semaphore->semaphore_value > 0){

		if(target_semaphore->semaphore_value < SEM_VALUE_MAX){

			target_semaphore->semaphore_value++;
			unlock();

		}else{

			perror("Error posting semaphore: value exceed maximum !\n");
			unlock();
			return -1;

		}

	}else{

		if(target_semaphore->waiting_list.empty() == true){
			
			target_semaphore->semaphore_value++;
			unlock();
		
		}else{
		
			pthread_t waiter_id = target_semaphore->waiting_list.front();
			TCB* waiter = find_thread_by_id(waiter_id);
			waiter->thread_state = TH_ACTIVE;

			target_semaphore->waiting_list.pop();
			unlock();
		}
	}

	return 0;	// If success
		
}