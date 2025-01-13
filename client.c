/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
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
    char hostbuffer[MAX_MESSAGE_LEN];
    char buffer[MAX_MESSAGE_LEN];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;
    static char returnMessage[MAX_MESSAGE_LEN];
    char ServerIP[INET_ADDRSTRLEN];
    char* ServerPort = malloc(sizeof(char)*256);

    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    IPbuffer = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));

    char menuBuffer[16];
    char menuIpBuffer[16];
    printf("Select an option \n1. For 255.255.255.255 broadcast with generic send message \n2. Specify server adress for connection \n3. Broadcast using teacher's method and current network broadcast adress\n");
    scanf("%s", menuBuffer);

    if(strcmp(menuBuffer, "1") == 0)
        SendMessage(SERVER_DISCOVERY_MESSAGE, "255.255.255.255", port, returnMessage, 1);
    else if(strcmp(menuBuffer, "2") == 0) 
    {
        printf("Specify the server IP adress: \n");
        scanf("%s", menuIpBuffer);
        SendMessage(SERVER_DISCOVERY_MESSAGE, menuIpBuffer, port, returnMessage, 1);
    }
    else if(strcmp(menuBuffer, "3") == 0)
        BroadcastSignIn(port, returnMessage);
    
    printf("Message recieved: \"%s\" \n", returnMessage);

    // Filter message
    if (returnMessage[0] == '#')
    {
        char *token = strtok(returnMessage, "#");

        //printf("Token: %s\n", token);
        if(strcmp(menuBuffer, "2") == 0)
            strcpy(ServerIP, menuIpBuffer);
        else
            memcpy(ServerIP, token, strlen(token) * sizeof(char) + 1);
        token = strtok(NULL, "#");
        //printf("Current Token: %s\n", token);
        memcpy(ServerPort, token, strlen(token) * sizeof(char) + 1);

///----------------------------------------

        int sockfd, n;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        char buf[MAX_MESSAGE_LEN] = "";
        CLIENT_INFO *newClient = NULL;

        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            printf("ERROR opening socket");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(atoi(ServerPort));
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        bzero(&(serv_addr.sin_zero), 8);
        
        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
            printf("ERROR on binding on ListenForClients");

///---------------------------------------
        // Consume input file
        while (1)
        {
            printf("Enter the next message: ");
            bzero(buffer, 256);
            // fgets(buffer, 256, stdin);
            scanf("%s", buffer);
            printf("Server IP: %s:%s(string) or %d(int)\n", ServerIP, ServerPort, atoi(ServerPort));
            SendMessage(buffer, menuIpBuffer, atoi(ServerPort), buffer, 1);
            n = recvfrom(sockfd, buf, MAX_MESSAGE_LEN, 0, (struct sockaddr *)&cli_addr, &clilen);
                if (n < 0)
            printf("ERROR on recvfrom");
            printf("Received a datagram: %s\n", buf);
        }
    
        close(sockfd);
    }
}