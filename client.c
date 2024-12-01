/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include "client.h"

char ServerIP[INET_ADDRSTRLEN];
char ServerPort[4];

void *ClientInputSubprocess()
{
    char userInput[4];

    system("clear");

    while (1)
    {
        printf("EXIT, SLEEP OR LOCAL (exit, sleep placeholder, localHost test)\nWaiting for user input: ");
        scanf("%s", userInput);
        system("clear");
        if (strcmp(userInput, "EXIT") == 0)
        {
            system("clear");
            printf("SHOULD EXIT NOW\n");
            exit(0);
        }
    }
}

void RunClient(int port)
{
    char hostbuffer[MAX_MESSAGE_LEN];
    char buffer[MAX_MESSAGE_LEN];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;
    static char returnMessage[MAX_MESSAGE_LEN];

    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    IPbuffer = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    // 127.0.0.1 é o IP redundante, conexão da própria maquina com ela mesma.


    char menuBuffer[16];
    printf("Select an option\n1. For 255.255.255.255 broadcast\n2. Specify server adress\n");
    scanf("%s", menuBuffer);

    if(strcmp(menuBuffer, "1") == 0)
        SendMessage(SERVER_DISCOVERY_MESSAGE, "255.255.255.255", port, returnMessage, 1);
    else 
    {
        printf("Specify the server IP adress: \n");
        scanf("%s", menuBuffer);
        SendMessage(SERVER_DISCOVERY_MESSAGE, menuBuffer, port, returnMessage, 1);
    }
    
    printf("Message recieved: \"%s\" \n", returnMessage);

    // Filter message
    if (returnMessage[0] == '#')
    {
        char *token = strtok(returnMessage, "#");

        //printf("Token: %s\n", token);
        memcpy(ServerIP, token, strlen(token) * sizeof(char) + 1);
        token = strtok(NULL, "#");
        //printf("Token: %s\n", token);
        memcpy(ServerPort, token, strlen(token) * sizeof(char));

        printf("Server IP: %s:%s\n", ServerIP, ServerPort);
        // Consume input file
        while (1)
        {
            printf("Enter the next message: ");
            bzero(buffer, 256);
            // fgets(buffer, 256, stdin);
            scanf("%s", buffer);
            SendMessage(buffer, "127.0.0.1", atoi(ServerPort), buffer, 1);
        }
    }
}