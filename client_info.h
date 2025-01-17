#ifndef _CLIENT_INFO_H_
#define _CLIENT_INFO_H_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include <netinet/in.h>
#include "constants.h"

typedef struct client_data {
    int client_simple_id;
    int client_id;
    int client_received_value[MAX_BUFFER];
    int is_connected;
    int newRequestValue;
    char IP[INET_ADDRSTRLEN];
    int port;
    int last_value;
    int last_req;        // Último ID de requisição
} CLIENT_INFO;

#endif // _CLIENT_INFO_H_
