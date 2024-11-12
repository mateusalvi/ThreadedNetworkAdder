#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define ID_BUFFER 32
#define MAX_CLIENTS 100
#define MAX_BUFFER 100


int max_ticket();
static void * thread_process(void *arg);
static void * client_thread(void *arg);
void server_functioning(int argc, char **argv);
int msg_to_client(char* message_to_send);
int wait_connection();
int wait_closure();
void client_input_value(int* buffer);
void wait_for_unlock(pthread_mutex_t mutex_A);
void wait_disconnect(CLIENT_INFO* this_client);
void wait_closure(int* connection_status);
void wait_connection();
void get_client_data(CLIENT_INFO* this_client);


typedef struct request_data{
	int		request_value;
	int		request_id;
	int		processed;
}	REQUEST_INFO;

typedef struct client_data{
	char	client_id[ID_BUFFER];
	REQUEST_INFO	request[MAX_BUFFER];
//Aqui foi apenas uma ideia para nao ter de trabalhar com o endereco MAC podemos
//fazer uma funcao que gere um id simplificado, mas se quiserem trabalhar com o 
//id completo tem de adaptar o codigo para trabalhar com strings
	int		client_simple_id;
	int		client_received_value[MAX_BUFFER];
	int		is_connected;
	int		last_value;
}	CLIENT_INFO;

