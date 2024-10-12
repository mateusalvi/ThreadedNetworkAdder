#ifndef _CLIENT_
#define _CLIENT_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"
#include "discovery.h"
#include "monitoring.h"

void HandleSystemSignals(int sig);

void* ClientCatchSignal(void* arg);

void* ClientInputSubprocess();

void* ClientListenForWakeup();

void RunClient();

#endif