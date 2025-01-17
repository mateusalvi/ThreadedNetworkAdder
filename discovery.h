#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>
#include "server_prot.h"

#define MAX_MESSAGE_LEN 256
#define MAX_CLIENTS 10

// Estrutura para armazenar informações do cliente
typedef struct {
    int id;
    char ip[INET_ADDRSTRLEN];
    int port;
} CLIENT_INFO;

// Funções de descoberta
void BroadcastSignIn(int port, char *returnMessage);
void ListenForNewClients(int port);
int ListenForAddRequest(int port, char *clientIP);
void SendMessage(char *message, char *ip, int port, char *returnMessage, bool expectReturn);
bool TestServerConnection(const char *ip, int port);

// Funções auxiliares
CLIENT_INFO NewClientStruct(int id, char *ip, int port);

#endif // DISCOVERY_H