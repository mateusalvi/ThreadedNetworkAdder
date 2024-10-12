#ifndef _MONITORING_
#define _MONITORING_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"

typedef struct __packet{
    uint16_t type; //Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn; //Número de sequência
    uint16_t length; //Comprimento do payload
    uint16_t timestamp; // Timestamp do dado
    const char* _payload; //Dados da mensagem
} packet;

typedef struct __sleepingPC
{
    char* ip;
    char* mac;
    struct __sleepingPC* next;
    int port;
    char* lastMessage;
} sleepingPC;

void Listen();

void AddNewClient(char* ip, char* mac, char* message);

sleepingPC* GetLastMachine(sleepingPC* root);

sleepingPC* FindParentPC(char* ip, char* mac, sleepingPC rootSleepingPC);

void PrintAllClients();

void PrintClients(sleepingPC* rootSleepingPC, int counter);

void WakePC(sleepingPC* client);

bool IsClientListInUse();

#endif