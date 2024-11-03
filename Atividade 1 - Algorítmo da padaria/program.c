// INF01151 - Sistemas Operacionais II N
// Mateus Luiz Salvi -

#include "lamport.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define SUMS 3000000

int main(int argc, char **argv)
{
    int thread_num, ret;
    pthread_t tinfo_process[N_THREADS];
    int tinfo_id[N_THREADS];
    pthread_attr_t attr;
    void *res;

    lamport_mutex_init();
    ret = pthread_attr_init(&attr);
    printf("\n%s\n", argv[1]);
    for (thread_num = 0; thread_num < N_THREADS; thread_num++)
    {
        tinfo_id[thread_num] = thread_num;
        if(strcmp(argv[1], "lamport") == 0)
            ret = pthread_create(&tinfo_process[thread_num], &attr, &lamport_thread_process, &tinfo_id[thread_num]);
        else if(strcmp(argv[1], "pthread") == 0)
            ret = pthread_create(&tinfo_process[thread_num], &attr, &hw_thread_process, &tinfo_id[thread_num]);
        else
        {
            printf("\nWrong parameter\n");
            return(EXIT_FAILURE);
        }
    }

    ret = pthread_attr_destroy(&attr);

    for (thread_num = 0; thread_num < N_THREADS; thread_num++)
    {
        ret = pthread_join(tinfo_process[thread_num], &res);
        //printf("Joined with thread id %d\n", thread_num);
        free(res);
    }

    printf("\nFinal accumulator value: %d \n", accumulator);

    exit(EXIT_SUCCESS);
}