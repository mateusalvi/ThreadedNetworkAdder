#ifndef _LAMPORT_
#define _LAMPORT_

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define N_THREADS 3
#define N_ITERACTIONS 3000000

int choosing [N_THREADS], ticket [N_THREADS], mutex_AT, accumulator;
pthread_mutex_t lock;

void lamport_mutex_init();
void lamport_mutex_lock (int thread_id);
void lamport_mutex_unlock (int thread_id);
void *lamport_thread_process(void *arg);
void *hw_thread_process(void *arg);
int max_ticket();

#endif