#ifndef _SERVER_
#define _SERVER_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"
#include "monitoring.h"
#include "discovery.h"

void RunServer();

void SignalWakeSubprocess();

void* ServerInputSubprocess();

void* ServerListenForSleepSubprocess();

void* SignalWake(char* ip, int port);

#endif