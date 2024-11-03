#ifndef _BAKERY_
#define _BAKERY_


//INF01151 - Sistemas Operacionais II N 
//Mateus Luiz Salvi - 



#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define N_THREADS 10
#define N_ITERACTIONS 3000

void lamport_mutex_init();
void lamport_mutex_lock (int thread_id);
void lamport_mutex_unlock (int thread_id);
void *thread_process(void *arg);

#endif