#ifndef SERVER_PROT_H
#define SERVER_PROT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "config.h"
#include "replication.h"

// Tipos de pacotes
typedef enum {
    DESC,       // Discovery request
    DESC_ACK,   // Discovery response
    DESC_SERVER, // Server discovery (broadcast)
    REQ,        // Request
    REQ_ACK     // Request response
} packet_type;

// Estrutura para pacotes de descoberta
typedef struct {
    int port;           // Porta para comunicação
} discovery_data;

// Estrutura para pacotes de requisição
typedef struct {
    long long seqn;     // Número de sequência
    int value;          // Valor a ser somado
} request_data;

// Estrutura para pacotes de resposta
typedef struct {
    long long seqn;     // Número de sequência
    int value;          // Soma atual
    int status;         // Status da operação
} response_data;

// União para os dados do pacote
typedef union {
    discovery_data disc;
    request_data req;
    response_data resp;
} packet_data;

// Estrutura do pacote
typedef struct {
    packet_type type;   // Tipo do pacote
    packet_data data;   // Dados do pacote
} packet;

// Funções exportadas
void init_server(int port);
void stop_server(void);
void *discovery_thread(void *arg);
void *request_thread(void *arg);

#endif // SERVER_PROT_H