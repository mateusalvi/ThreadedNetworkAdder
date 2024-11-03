#ifndef _LAMPORT_
#define _LAMPORT_

// INF01151 - Sistemas Operacionais II N
// Mateus Luiz Salvi – 00229787
// Adilson Enio Pierog – 00158803
// Andres Grendene Pacheco - 00264397
// Luís Filipe Martini Gastmann - 00276150


#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>

#define N_THREADS 3
#define N_ITERACTIONS 3000000

int ticket [N_THREADS], accumulator;
bool choosing [N_THREADS];
pthread_mutex_t lock;

void lamport_mutex_init();
void lamport_mutex_lock (int thread_id);
void lamport_mutex_unlock (int thread_id);
void *lamport_thread_process(void *arg);
void *hw_thread_process(void *arg);
int max_ticket();

#endif