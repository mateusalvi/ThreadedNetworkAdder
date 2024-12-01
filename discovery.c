/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#define _GNU_SOURCE /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include "discovery.h"

int clientsCount = 0;
char MyIP[INET_ADDRSTRLEN];
CLIENT_INFO clients[MAX_CLIENTS];

// Subnet mask getter, thanks to https://stackoverflow.com/questions/18100761/obtaining-subnetmask-in-c
int get_addr_and_netmask_using_ifaddrs(const char *ifa_name, char *addr, char *netmask)
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char *s;
    int found = 0;

    if (getifaddrs(&ifap) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifap; ifa && !found; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        if (strcasecmp(ifa_name, ifa->ifa_name))
            continue;

        /* IPv4 */
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        sa = (struct sockaddr_in *)ifa->ifa_addr;
        s = inet_ntoa(sa->sin_addr);
        strcpy(addr, s);

        sa = (struct sockaddr_in *)ifa->ifa_netmask;
        s = inet_ntoa(sa->sin_addr);
        strcpy(netmask, s);

        found = 1;
    }

    freeifaddrs(ifap);

    if (found)
        return EXIT_SUCCESS;
    return EXIT_FAILURE;
}

char *GetBroadcastAdress()
{
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *sin;
    char *broadcast_addr = malloc(INET_ADDRSTRLEN);

    // Create a socket to get interface address
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Get the interface address
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0)
    {
        perror("ioctl failed to get IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    char *ip_addr = inet_ntoa(sin->sin_addr);

    // Get the netmask
    if (ioctl(sockfd, SIOCGIFNETMASK, &ifr) < 0)
    {
        perror("ioctl failed to get netmask");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    char *netmask = inet_ntoa(sin->sin_addr);

    // Calculate the broadcast address
    struct in_addr addr, mask, broadcast;
    inet_aton(ip_addr, &addr);
    inet_aton(netmask, &mask);
    broadcast.s_addr = addr.s_addr | (~mask.s_addr);

    // Convert broadcast address to string
    strcpy(broadcast_addr, inet_ntoa(broadcast));

    close(sockfd);
    printf("Broadcast address is: %s \n", broadcast_addr);
    return broadcast_addr;
}

void SendMessage(char *message, char *ip, int port, char *returnMessage, bool expectReturn)
{
    int sockfd, n;
    unsigned int length;
    struct sockaddr_in serv_addr, from;
    struct hostent *server;

    // if (argc < 2) {
    // 	fprintf(stderr, "usage %s hostname\n", argv[0]);
    // 	exit(0);
    // }
    server = gethostbyname(ip); //?????????????????????????????????????????????
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        printf("ERROR opening socket\n");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);
    printf("Sending \"%s\" to \"%s:%d\"\n", message, ip, port);
    n = sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
    if (n < 0)
    {
        printf("ERROR sendto\n");
        exit(EXIT_FAILURE);
    }
    printf("Request \"%s\" sent\n", message);
    if (expectReturn)
    {
        printf("Waiting for response...\n");
        length = sizeof(struct sockaddr_in);
        n = recvfrom(sockfd, returnMessage, 256, 0, (struct sockaddr *)&from, &length);
        if (n < 0)
            printf("ERROR recvfrom ");
    }

    close(sockfd);
}

CLIENT_INFO *ListenForNewClients(int port)
{
    int sockfd, n;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    char buf[MAX_MESSAGE_LEN] = "";
    CLIENT_INFO *newClient = NULL;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        printf("ERROR opening socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);

    // Copy ip adress to MyIP constant
    inet_ntop(AF_INET, &serv_addr, MyIP, INET_ADDRSTRLEN);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
        printf("ERROR on binding");

    clilen = sizeof(struct sockaddr_in);

    // while (1)
    // {
    /* receive from socket */
    printf("Listening for new clients...\n");
    n = recvfrom(sockfd, buf, MAX_MESSAGE_LEN, 0, (struct sockaddr *)&cli_addr, &clilen);

    if (n < 0)
        printf("ERROR on recvfrom");
    printf("Received a datagram: %s\n", buf);

    /* send to socket */
    // n = sendto(sockfd, "Got your message\n", 17, 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
    // if (n < 0)
    //     printf("ERROR on sendto");

    // Se a menssagem recebida for a de SERVER DISCOVERY
    if (strcmp(buf, SERVER_DISCOVERY_MESSAGE) == 0)
    {
        char message[MAX_MESSAGE_LEN] = "";
        char _port[4];
        strcat(message, "#");
        strcat(message, MyIP);
        strcat(message, "#");
        if (sprintf(_port, "%d", port+1+clientsCount) < 0)
            printf("ERRO NO SPRINTF!\n");
        strcat(message, _port);
        // strcat(message, "\0");
        printf("Sending response: \"%s\"\n", message);
        // Responde ao novo cliente.
        n = sendto(sockfd, &message, MAX_MESSAGE_LEN, 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
        if (n < 0)
            printf("ERROR on sendto");
        else
        {
            newClient = AddNewClient((struct sockaddr *)&cli_addr, port);
        }
    }

    close(sockfd);

    return newClient;
}

int ListenForAddRequest(int port, char *clientIP)
{
    int sockfd, n;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    char buf[256];
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        printf("ERROR opening socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    //serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);
    inet_pton(AF_INET, clientIP, &(cli_addr.sin_addr.s_addr));

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
        printf("ERROR on binding");

    clilen = sizeof(struct sockaddr_in);

    char message[MAX_MESSAGE_LEN] = "";
    // strcat(message, MyIP);

    /* receive from socket */
    printf("Listening for requests from %s:%d \n", clientIP, port);
    n = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *)&cli_addr, &clilen);
    if (n < 0)
        printf("ERROR on recvfrom");

    printf("Received a request from %s:%d of +%s\n", clientIP, port, buf);

    /* send to socket */
    // n = sendto(sockfd, "Got your message\n", 17, 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
    // if (n < 0)
    //     printf("ERROR on sendto");

    // Se a menssagem recebida for a de SERVER DISCOVERY
    //  if(strcmp(buf, SERVER_DISCOVERY_MESSAGE) == 0)
    //  {
    // Responde ao novo cliente.
    // n = sendto(sockfd, message, MAX_MESSAGE_LEN, 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
    // if (n < 0)
    //     printf("ERROR on sendto");

    close(sockfd);
    return atoi(buf);
}

// Returns the new client Index (ID)
CLIENT_INFO *AddNewClient(struct sockaddr *cli_addr, int port)
{
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr, clientIP, INET_ADDRSTRLEN);

    clientsCount++;
    int currentPort = (port + clientsCount);
    memcpy(&clients[clientsCount], NewClientStruct(clientsCount - 1, clientIP, currentPort), sizeof(CLIENT_INFO));
    printf("Added new client: %s:%d\n", clientIP, currentPort);

    return &clients[clientsCount];
}

// void DefineServerIP(char *newServerIP, char *port)
// {
//     strcpy(ServerIP, newServerIP);
//     strcpy(ServerPort, port);
// }

CLIENT_INFO *GetClientsVector()
{
    CLIENT_INFO *copyOfClients[MAX_CLIENTS];
    pthread_mutex_lock(&mutexClientList);
    memcpy(&copyOfClients, &clients, sizeof(CLIENT_INFO) * MAX_CLIENTS);
    pthread_mutex_unlock(&mutexClientList);
    return *copyOfClients;
}

CLIENT_INFO *NewClientStruct(int id, char *ip, int port)
{
    CLIENT_INFO *p = malloc(sizeof(CLIENT_INFO));
    p->client_id = id;
    p->last_value = 0;
    strcpy(p->IP, ip);
    p->port = port;
    p->is_connected = 1;
    return p;
}