//INF01151 - Sistemas Operacionais II N 
//Mateus Luiz Salvi - 

#include "libbakery.h"

int choosing [N_THREADS];
int ticket [N_THREADS];
int mutex_AT, accumulator;

void lamport_mutex_init()
{
	mutex_AT = 0;
	accumulator = 0;
}

void lamport_mutex_lock (int thread_id)
{
	mutex_AT = 1;
}

void lamport_mutex_unlock (int thread_id)
{
	mutex_AT = 0;
}

void dormir() 
{
  usleep((rand() % 11) * 100000); /* dormir 0, 100, 200, ..., 1000 milissegundos */
}

int max_ticket()
{
  int i, max = ticket[0];

  for (i = 1; i < N_THREADS; i++)
    max = ticket[i] > max ? ticket[i] : max;
}

void *thread_process(void *arg)
{
  int j, i = *((int *) arg);

  printf("Hello! I'm thread %d!\n", i);

  do{
    choosing[i] = 1;
    ticket[i] = max_ticket () + 1;
    choosing[i] = 0;

    for (j = 0; j < N_THREADS; j++) 
    {
      while (choosing[j])
        /* nao fazer nada */;
      while (ticket[j] != 0 && ((ticket[j] < ticket[i]) || (ticket[j] == ticket[i] && j < i)))
        /* nao fazer nada */;
    }
	
/*ALTERAÇAO - AREA CRITICA */	
/* Lock mutex, locked_mutex = 1 */
	
	lamport_mutex_lock(i);
	if(mutex_AT != 1) printf("An error ocurred in the critical region \n"); //backup
	else{
    printf("I'm thread %d and I'm entering my critical region!, accumulator value = %d \n", i, accumulator);
	
	accumulator++; //Accumulator for N_THREADS, counting from 0 to N_ITERACTIONS
	
	
    printf("I'm thread %d and I'm leaving my critical region!\n", i);
	}
	lamport_mutex_unlock(i);
/* Unlock mutex, locked_mutex = 0*/ 
	

    ticket[i] = 0; /* indicar que saimos da secao critica */

  }while (accumulator <= N_ITERACTIONS);

  return NULL;
}