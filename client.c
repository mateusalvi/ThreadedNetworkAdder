/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "client.h"

const char* myIp;
const char* serverIp;

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
    }
}

void RunClient(int port)
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
    SendMessage(SERVER_DISCOVERY_MESSAGE, "127.0.0.1", port); //127.0.0.1 é o IP redundante, conexão da própria maquina com ela mesma.


    // SendMessage("Are you the server?", IPbuffer, port);

    //BroadcastSleep("127.0.0.1");

    // //Start Signal Catcher and Input Reader threads
    // pthread_create(&threads[THREAD_CLIENT_INPUT], NULL, ClientInputSubprocess, &signalArg);
    // pthread_create(&threads[THREAD_CLIENT_SIGNAL_CATCHER], NULL, ClientCatchSignal, &inputArg);


    // //Wait for Signal Catcher and Input Reader threads
    // pthread_join(threads[THREAD_CLIENT_INPUT], NULL);
    // pthread_join(threads[THREAD_CLIENT_SIGNAL_CATCHER], NULL);

}