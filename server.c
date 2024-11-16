/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "server.h"

int sendMessageLock = 0;

void RunServer()
{
    int monitoringArg = 0;  
    pthread_create(&threads[THREAD_SERVER_MONITORING], NULL, ServerListenForSleepSubprocess, &monitoringArg);
    pthread_create(&threads[THREAD_SERVER_INPUT], NULL, ServerInputSubprocess, &monitoringArg);

    pthread_join(threads[THREAD_SERVER_MONITORING], NULL);
    pthread_join(threads[THREAD_SERVER_INPUT], NULL);
}

void* SignalWake(char* ip, int port)
{
    SendMessage("HELLO IM THE SERVER","127.0.0.1", 1); //(MAC ADRESS * 16) + MAGIC_PACKAGE_SUFIX is the magic package
}

void SignalWakeSubprocess()
{
    int shouldRun = 1;
    while (shouldRun > 0)
    {
        if(sendMessageLock > 0)
            continue;
        else
        {
            pthread_create(&threads[THREAD_SERVER_WAKE_SIGNAL], NULL, SignalWake, NULL);
            pthread_join(threads[THREAD_SERVER_WAKE_SIGNAL], NULL);
            shouldRun = 0;
        }
    }
}

void* ServerInputSubprocess()
{
    char userInput[20];
    regex_t reegex;

    int regexBool = regcomp(&reegex, "WAKE^([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))."
             "([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))."
             "([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))."
             "([0-9]|[1-9][0-9]|1([0-9][0-9])|2([0-4][0-9]|5[0-5]))$", REG_EXTENDED);

    if(regexBool)
    {
        printf("Could not compile regex\n");
    }
    
    while(1)
    {
        printf("Use \"WAKE 255.255.255.255\"\n");
        scanf("%19[^\n]", userInput); 

        // regexBool = regexec(&reegex, userInput, 0, NULL, 0);
        // if (!regexBool)
        // {
            system("clear");
            //printf("Waking machine: %s\n", userInput);
            printf("Sending packet to localhost");
            SignalWakeSubprocess();
        // }
    }

    regfree(&reegex);
}


void* ServerListenForSleepSubprocess()
{
    while(1)
    {
        ListenForSleepBroadcasts(threads);
        printf("Should add to table\n");
    }
}