#ifndef _BAKERY_
#define _BAKERY_

/*##########################################################
#                                                          #
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#           Mateus Luiz Salvi - Bianca Pelegrini           #
#                                                          #
##########################################################*/

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define N_THREADS 10

void lamport_mutex_init();
void lamport_mutex_lock (int thread_id);
void lamport_mutex_unlock (int thread_id);
void *thread_process(void *arg);

#endif