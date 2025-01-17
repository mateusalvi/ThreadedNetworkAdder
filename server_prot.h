#ifndef SERVER_PROT_H
#define SERVER_PROT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define MAX_MESSAGE_LEN 256
#define SOCKET_TIMEOUT_MS 100  // 100ms timeout

// Tipos de pacotes
typedef enum {
    DESC,       // Descoberta de servidor
    DESC_ACK,   // Resposta de descoberta
    REQ,        // Requisição de adição
    REQ_ACK     // Resposta de adição
} packet_type;

// Estrutura de requisição
typedef struct {
    long long seqn;    // Número de sequência
    int value;         // Valor a ser somado
} request_data;

// Estrutura de resposta
typedef struct {
    long long seqn;    // Número de sequência
    int value;         // Valor atual da soma
    int status;        // Status da operação (0 = sucesso, outros = erro)
} response_data;

// Estrutura do pacote
typedef struct {
    packet_type type;  // Tipo do pacote
    union {
        request_data req;    // Dados da requisição
        response_data resp;  // Dados da resposta
    } data;
} packet;

// Funções do servidor
void ServerMain(const char* port);
void start_server(int port);
void discovery_service(int port);
void request_service(int port);
int receive_and_decode_message(int sockfd, packet *received_packet, struct sockaddr_in *client_addr);

#endif // SERVER_PROT_H