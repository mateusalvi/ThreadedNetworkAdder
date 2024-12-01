#ifndef _DISCOVERY_
#define _DISCOVERY_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"

typedef struct client_data
{
	int client_id;
	//REQUEST_INFO request[MAX_BUFFER];
	// Aqui foi apenas uma ideia para nao ter de trabalhar com o endereco MAC podemos
	// fazer uma funcao que gere um id simplificado, mas se quiserem trabalhar com o
	// id completo tem de adaptar o codigo para trabalhar com strings
	int client_simple_id;
	int client_received_value[MAX_BUFFER];
	int is_connected;
	char IP[INET_ADDRSTRLEN];
	int port;
	int last_value;
} CLIENT_INFO;

CLIENT_INFO *NewClientStruct(int id, char* ip, int port);

pthread_mutex_t mutexClientList;

char *GetBroadcastAdress();

void SendMessage(char *message, char *ip, int port, char *returnMessage, bool expectReturn);

CLIENT_INFO* ListenForNewClients(int port);

CLIENT_INFO* AddNewClient(struct sockaddr* cli_addr, int port);

void StartClientListener(int currentClientIndex);

void AnswerNewClient(char* message, int sockfd, struct sockaddr* cli_addr);

int ListenForAddRequest(int port, char* ip);

CLIENT_INFO* GetClientsVector();

#endif