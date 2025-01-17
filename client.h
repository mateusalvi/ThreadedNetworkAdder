#ifndef _CLIENT_
#define _CLIENT_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

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
#include "processing.h"
#include "discovery.h"

// Função para executar o cliente
void RunClient(int port);

// Função para processar entrada do usuário
void* ClientInputSubprocess(void* arg);

#endif