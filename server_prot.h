#ifndef _SERVERPROT_
#define _SERVERPROT_

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "discovery.h"
#include "constants.h"

#define ID_BUFFER 32



typedef struct request_data
{
	int request_value;
	int request_id;
	int processed;
} REQUEST_INFO;

// typedef struct client_data
// {
// 	char client_id[ID_BUFFER];
// 	REQUEST_INFO request[MAX_BUFFER];
// 	// Aqui foi apenas uma ideia para nao ter de trabalhar com o endereco MAC podemos
// 	// fazer uma funcao que gere um id simplificado, mas se quiserem trabalhar com o
// 	// id completo tem de adaptar o codigo para trabalhar com strings
// 	int client_simple_id;
// 	int client_received_value[MAX_BUFFER];
// 	int is_connected;
// 	int last_value;
// } CLIENT_INFO;

void *AckSumRequest_Consumer(void *arg);
void *SumRequest_Producer(void *arg);
static void *ClientHandlerThread(void *arg);
int max_ticket();
static void *thread_process(void *arg);
static void *ClientHandlerThread(void *arg);
void ServerMain(char* port);
void return_value_to_client(int value);
int msg_to_client(char *message_to_send);
int wait_closure();
void client_input_value(int *buffer);
void wait_for_unlock(pthread_mutex_t mutex_A);
void wait_disconnect(CLIENT_INFO *this_client);
void RegisterNewClient(CLIENT_INFO *this_client);
void NetworkListenerSubprocess(char* port);
// void* AddNewClient(CLIENT_INFO *newClient);

#endif