/*##########################################################
#                                                          #
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#           Mateus Luiz Salvi - Bianca Pelegrini           #
#                                                          #
##########################################################*/

#include "libbakery.h"
#include <pthread.h>

#define SUMS 3000000

int main(int argc, char **argv)
{
  int thread_num, ret;
  int tinfo_id[N_THREADS];
  int *accumulator;
  pthread_t tinfo_process[N_THREADS];
  pthread_attr_t attr;
  void *res;

  ret = pthread_attr_init(&attr);

  for (thread_num = 0; thread_num < N_THREADS; thread_num++)
  {
    tinfo_id [thread_num] = thread_num;
    ret = pthread_create(&tinfo_process[thread_num], &attr, &thread_process, &tinfo_id[thread_num]);
  }

  ret = pthread_attr_destroy(&attr);

  for(thread_num = 0; thread_num < N_THREADS; thread_num++)
  {
    ret = pthread_join(tinfo_process[thread_num], &res);
    printf("Joined with thread id %d\n", thread_num);
    free(res);
  }

  exit(EXIT_SUCCESS);
}

void ThreadSum(int *accumulator)
{
    for (size_t i = 0; i < SUMS; i++)
    {
        accumulator++;
    }
}