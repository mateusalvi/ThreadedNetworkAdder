/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "client.h"

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
    char hostbuffer[256];
    char buffer[256];
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
    SendMessage(SERVER_DISCOVERY_MESSAGE, "255.255.255.255", port, returnMessage);
    printf("Message recieved: \"%s\" \n", returnMessage);

    // Filter message
    if (returnMessage[0] == '#')
    {
        char *token = strtok(returnMessage, "#");

        printf("Token: %s\n", token);
        memcpy(ServerIP, token, strlen(token) * sizeof(char) + 1);
        token = strtok(NULL, "#");
        printf("Token: %s\n", token);
        memcpy(ServerPort, token, strlen(token) * sizeof(char));

        printf("Server IP: %s:%s\n", ServerIP, ServerPort);
        // Consume input file
        while (1)
        {
            printf("Enter the next message: ");
            bzero(buffer, 256);
            // fgets(buffer, 256, stdin);
            scanf("%s", buffer);
            SendMessage(buffer, ServerIP, atoi(ServerPort), returnMessage);
        }
    }
}