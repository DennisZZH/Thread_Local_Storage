//Author: Zihao Zhang
//Date: 5.22.2019

#include "threads.h"
#include <vector>

using namespace std;

int tls_create(unsigned int size);

int tls_write(unsigned int offset, unsigned int length, char *buffer);

int tls_read(unsigned int offset, unsigned int length, char *buffer);

int tls_destroy();

int tls_clone(pthread_t tid);

void* tls_get_internal_start_address();


// Page Data Structure
typedef struct {
	// Define any fields you might need inside here.
	unsigned long page_address;
	int reference_count;
}Page;


// TLS Data Structure
typedef struct {
	// Define any fields you might need inside here.
	pthread_t tls_id;
	unsigned int tls_size;
    unsigned int page_num;
	vector<Page*> pages;
}TLS;

