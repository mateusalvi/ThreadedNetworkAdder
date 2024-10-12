#ifndef _DISCOVERY_
#define _DISCOVERY_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"
#include "monitoring.h"

typedef struct __SleepMessageQueue
{
    char *message;
    struct __SleepMessageQueue *nextMessage;
} SleepMessageQueue;

char* GetBroadcastAdress();

void* BroadcastSleep(char* broadcastAdress);

static void Broadcast(const char *message);

void* ListenForSleepBroadcast();

int ListenForSleepBroadcasts(pthread_t *threads);

void* SendMessage(char* _message, char* ip, int port);

#endif