#include "lamport.h"

void lamport_mutex_init()
{
  mutex_AT = 0;
  accumulator = 0;

  for (size_t i = 0; i < N_THREADS; i++)
  {
    choosing[i] = 0; 
    ticket[i] = 0;
  }
}

void lamport_mutex_lock(int i)
{
  choosing[i] = 1;
  ticket[i] = max_ticket() + 1;
  choosing[i] = 0;

  for (int j = 0; j <= N_THREADS; j++)
  {
    while (choosing[j])
    { 
    }
    while (ticket[j] != 0 && ((ticket[j] < ticket[i]) || (ticket[j] == ticket[i] && j < i)))
    { 
    }
  }
}

void lamport_mutex_unlock(int thread_id)
{
  ticket[thread_id] = 0;
}

int max_ticket()
{
  int i, max = ticket[0];

  for (i = 1; i < N_THREADS; i++)
    max = ticket[i] > max ? ticket[i] : max;
}

void *lamport_thread_process(void *arg)
{
  int i = *((int *)arg);
  int shouldRun = 1;
  //printf("Hello! I'm thread %d!\n", i);
  
  do{
    lamport_mutex_lock(i);

    //printf("I'm thread %d and I'm entering  my critical region!\n", i);
    if(accumulator < N_ITERACTIONS)
      accumulator++;
    else
      shouldRun = 0;
    //printf("I'm thread %d and I'm leaving my critical region!\n", i);

    lamport_mutex_unlock(i);
  }while (shouldRun > 0);

  return NULL;
}

void *hw_thread_process(void *arg)
{
  int i = *((int *)arg);
  int shouldRun = 1;
  //printf("Hello! I'm thread %d!\n", i);
  
  do{
    pthread_mutex_lock(&lock);
    
    if(accumulator < N_ITERACTIONS)
      accumulator++;
    else
      shouldRun = 0;

    pthread_mutex_unlock(&lock);
  }while (shouldRun > 0);

  return NULL;
}