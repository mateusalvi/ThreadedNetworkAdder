/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "client.h"

const char* myIp;
const char* serverIp;

void HandleSystemSignals(int sig)
{ 
    switch (sig)
    {
        case 2: //Code 2 is for OS calling EXIT
            system("clear");
            printf("OS Signal %d recieved. Exiting.\n", sig);
            exit(0);
            break;
        
        default:
            printf("Caught signal %d\n", (int)sig);
            break;
    }
}

void* ClientCatchSignal(void* arg)
{
    bool shouldRun = true;

    while (shouldRun) 
    { 
        signal(SIGINT, *HandleSystemSignals);
        signal(SIGSEGV, *HandleSystemSignals);
        signal(SIGILL, *HandleSystemSignals);
        signal(SIGFPE, *HandleSystemSignals);
        signal(SIGTERM, *HandleSystemSignals);
    } 

    pthread_exit(&arg);
}

void* ClientInputSubprocess()
{
    char userInput[4];

    system("clear");

    while(1)
    {
        printf("EXIT, SLEEP OR LOCAL (exit, sleep placeholder, localHost test)\nWaiting for user input: ");
        scanf("%s", userInput);
        system("clear");
        if(strcmp(userInput, "EXIT") == 0)
        {
            system("clear");
            printf("SHOULD EXIT NOW\n");
            exit(0);
        }
        else if(strcmp(userInput, "SLEEP") == 0)
        {
            char* broadCastAdress = GetBroadcastAdress();
            printf("Broadcasting SLEEP MODE to ip: %s\n", broadCastAdress);
            BroadcastSleep(broadCastAdress);
        }
        else if(strcmp(userInput, "LOCAL") == 0)
        {
            printf("Broadcasting SLEEP MODE to localHost 127.0.0.1\n");
            BroadcastSleep("127.0.0.1");
        }
    }
}

void* ClientListenForWakeup()
{
    ListenForSleepBroadcasts(threads);
}

void RunClient()
{
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;
 
    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));


    int signalArg, inputArg;

    SendMessage("sleep service discovery", IPbuffer, BROADCAST_PORT);
    //BroadcastSleep("127.0.0.1");

    //Start Signal Catcher and Input Reader threads
    pthread_create(&threads[THREAD_CLIENT_INPUT], NULL, ClientCatchSignal, &signalArg);
    pthread_create(&threads[THREAD_CLIENT_SIGNAL_CATCHER], NULL, ClientInputSubprocess, &inputArg);
    pthread_create(&threads[THREAD_CLIENT_WAKEUP_CATCHER], NULL, ClientInputSubprocess, &inputArg);

    //Wait for Signal Catcher and Input Reader threads
    pthread_join(threads[THREAD_CLIENT_INPUT], NULL);
    pthread_join(threads[THREAD_CLIENT_SIGNAL_CATCHER], NULL);
    pthread_join(threads[THREAD_CLIENT_WAKEUP_CATCHER], NULL);
}