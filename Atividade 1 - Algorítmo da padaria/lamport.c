#include "lamport.h"

int main(int argc, char **argv)
{
    int thread_num, ret;
    pthread_t tinfo_process[N_THREADS];
    int tinfo_id[N_THREADS];
    pthread_attr_t attr;
    void *res;
    
	lamport_mutex_init();
    ret = pthread_attr_init(&attr);
    
    for (thread_num = 0; thread_num < N_THREADS; thread_num++) {
	    tinfo_id [thread_num] = thread_num;
	    ret = pthread_create(&tinfo_process[thread_num], &attr, &thread_process, &tinfo_id[thread_num]);
    }

    ret = pthread_attr_destroy(&attr);
    
    for (thread_num = 0; thread_num < N_THREADS; thread_num++) {
    	ret = pthread_join(tinfo_process[thread_num], &res);
    	printf("Joined with thread id %d\n", thread_num);
    	free(res);
    }

    exit(EXIT_SUCCESS);
}