#ifndef _CONSTANTS_
#define _CONSTANTS_

/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <sys/socket.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>

#define MAXTHREADS 100
#define BROADCAST_PORT 20000
#define MAX_MESSAGE_LEN 100
#define CLIENT_SIGNIN_MESSAGE "s#i#g#n#i#n"
#define MAGIC_PACKAGE_SUFIX "\xff\xff\xff\xff\xff\xff"

pthread_t threads[MAXTHREADS];

enum THREADS
{
    THREAD_CLIENT_INPUT,
    THREAD_CLIENT_SIGNAL_CATCHER,
    THREAD_CLIENT_WAKEUP_CATCHER,
    THREAD_SERVER_MONITORING,
    THREAD_SERVER_SLEEP_SOCKET,
    THREAD_SERVER_INPUT,
    THREAD_SERVER_WAKE_SIGNAL
};

#endif