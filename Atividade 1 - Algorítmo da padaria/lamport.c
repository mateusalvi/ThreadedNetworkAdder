// INF01151 - Sistemas Operacionais II N
// Mateus Luiz Salvi – 00229787
// Adilson Enio Pierog – 00158803
// Andres Grendene Pacheco - 00264397
// Luís Filipe Martini Gastmann - 00276150

#include "lamport.h"

void lamport_mutex_init()
{
  accumulator = 0;
  int i;
  for (i = 0; i < N_THREADS; i++)
  {
    choosing[i] = false;
    ticket[i] = 0;
  }
}

void lamport_mutex_lock(int i)
{
  choosing[i] = true;
  ticket[i] = max_ticket() + 1;
  choosing[i] = false;
  int j;
  for (j = 0; j < N_THREADS; j++)
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
  int interactionsCounter = 0;

  lamport_mutex_lock(i);

  do
  {
    accumulator++;
    interactionsCounter++;
  } while (interactionsCounter != N_ITERACTIONS);

  lamport_mutex_unlock(i);

  return NULL;
}

void *hw_thread_process(void *arg)
{
  int i = *((int *)arg);
  int interactionsCounter = 0;

  pthread_mutex_lock(&lock);

  do
  {
    accumulator++;
    interactionsCounter++;
  } while (interactionsCounter != N_ITERACTIONS);

  pthread_mutex_unlock(&lock);

  return NULL;
}