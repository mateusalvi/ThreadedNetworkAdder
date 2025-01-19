#ifndef CLIENT_H
#define CLIENT_H

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "server_prot.h"
#include "processing.h"
#include "discovery.h"

// Função para executar o cliente
void RunClient(int port);

// Função para processar entrada do usuário
void* ClientInputSubprocess(void* arg);

void ClientMain(const char* port);

#endif // CLIENT_H