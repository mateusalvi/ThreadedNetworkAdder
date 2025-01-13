/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#define _GNU_SOURCE /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include "discovery.h"

int clientsCount = 0;
char MyIP[INET_ADDRSTRLEN];
CLIENT_INFO clients[MAX_CLIENTS];
pthread_mutex_t mutexClientList;

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

void BroadcastSignIn(int port, char *returnMessage)
{
    int sockfd, n;
	unsigned int length;
	struct sockaddr_in serv_addr, from;
	struct hostent *server;
	
	server = gethostbyname(GetBroadcastAdress());
    
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }	
	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(port);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);  

    char signInMessage[12] = SERVER_DISCOVERY_MESSAGE;
    printf("SENDING MESSAGE: %s\n", signInMessage);
	n = sendto(sockfd, signInMessage, strlen(signInMessage), 0, (const struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
	if (n < 0) 
		printf("ERROR sendto");

	char buffer[256];
    bzero(buffer, 256);
	fgets(buffer, 256, stdin);
	length = sizeof(struct sockaddr_in);
	n = recvfrom(sockfd, returnMessage, sizeof(returnMessage), 0, (struct sockaddr *) &from, &length);
	if (n < 0)
		printf("ERROR recvfrom");

	printf("Got an ack: %s\n", buffer);
	
	close(sockfd);
}

void SendMessage(char *message, char *ip, int port, char *returnMessage, bool expectReturn)
{
    int sockfd, n;
    unsigned int length;
    struct sockaddr_in dest_Addr, from, my_addr;
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

    // // BIND SOCKET
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = INADDR_ANY;  

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) 
		perror("ERROR on binding");

    
    dest_Addr.sin_family = AF_INET;
    dest_Addr.sin_port = htons(port);
    dest_Addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(dest_Addr.sin_zero), 8);

    printf("Sending \"%s\" to \"%s:%d with hostname \"%s\"\n", message, ip, ntohs(dest_Addr.sin_port), server->h_name);
    n = sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *)&dest_Addr, sizeof(struct sockaddr_in));
    if (n < 0)
    {
        printf("ERROR sendto\n");
        exit(EXIT_FAILURE);
    }
    printf("Message \"%s\" sent\n", message);
    if (expectReturn)
    {
        printf("Waiting for response from %s:%d...\n", server->h_name, ntohs(dest_Addr.sin_port));
        length = sizeof(struct sockaddr_in);
        n = recvfrom(sockfd, returnMessage, 256, 0, (struct sockaddr *)&from, &length);
        if (n < 0)
            printf("ERROR recvfrom ");
        printf("Received a datagram: %s\n", returnMessage);
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
    //inet_ntop(AF_INET, &serv_addr, MyIP, INET_ADDRSTRLEN);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) 
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
        printf("ERROR on binding on ListenForClients");

    clilen = sizeof(struct sockaddr_in);

    // while (1)
    // {
    /* receive from socket */

    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    // To convert an Internet network
    // address into ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));

    printf("Hostname: %s\n", hostbuffer);
    strcpy(MyIP, IPbuffer);
    printf("Host IP: %s\n", MyIP);
    printf("Listening for new clients...\n");

    //printf("Listening for new clients...\n");
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
        char _port[5];
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
            char ip[16];
            inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
            newClient = AddNewClient(ip, port+1+clientsCount);
        }
    }

    close(sockfd);

    return newClient;
}

int ListenForAddRequest(int port, char *clientIP)
{
    // int sockfd, n;
    // socklen_t clilen;
    // struct sockaddr_in serv_addr, cli_addr;
    // char buf[256];
    // if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    //     printf("ERROR opening socket");

    // serv_addr.sin_family = AF_INET;
    // serv_addr.sin_port = htons(port);
    // //serv_addr.sin_addr.s_addr = INADDR_ANY;
    // bzero(&(serv_addr.sin_zero), 8);
    // inet_pton(AF_INET, clientIP, &(cli_addr.sin_addr.s_addr));
    // //printf("Trying to bind to %s:%d\n", clientIP, port);

    // int opt = 1;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) 
    // {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }
    
    // if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0)
    // {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }

    // if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
    //     printf("Error on binding at listen for add request method\n");

    // clilen = sizeof(struct sockaddr_in);

    // char message[MAX_MESSAGE_LEN] = "";
    // // strcat(message, MyIP);

    // /* receive from socket */
    // printf("Listening for requests from %s:%d \n", clientIP, port);
    // n = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *)&cli_addr, &clilen);
    // if (n < 0)
    //     printf("ERROR on recvfrom");

    // printf("Received a request from %s:%d of +%s\n", clientIP, port, buf);

    // /* send to socket */
    // // n = sendto(sockfd, "Got your message\n", 17, 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
    // // if (n < 0)
    // //     printf("ERROR on sendto");

    // // Se a menssagem recebida for a de SERVER DISCOVERY
    // //  if(strcmp(buf, SERVER_DISCOVERY_MESSAGE) == 0)
    // //  {
    // // Responde ao novo cliente.
    // // n = sendto(sockfd, message, MAX_MESSAGE_LEN, 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
    // // if (n < 0)
    // //     printf("ERROR on sendto");

    // close(sockfd);
    // return atoi(buf);
}

void *addRequestListenerThread(void *arg)
{
    CLIENT_INFO* thisClient = ((CLIENT_INFO *)arg);
    int sockfd, n;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    char buf[256];
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        printf("ERROR opening socket");

    printf("This client port: %d\n", thisClient->port);

    // // BIND SOCKET
	memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(thisClient->port);
    serv_addr.sin_addr.s_addr = INADDR_ANY; 
    bzero(&(serv_addr.sin_zero), 8);

    // inet_pton(AF_INET, thisClient->IP, &(cli_addr.sin_addr.s_addr));

    // int opt = 1;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) 
    // {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }
    
    // if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0)
    // {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
        printf("Error on binding at listen for add request method\n");

    clilen = sizeof(struct sockaddr_in);

    char message[MAX_MESSAGE_LEN] = "";

    while (thisClient->is_connected > 0)
    {
        bzero(buf, sizeof(buf));
        printf("Listening for requests from %s:%d \n", thisClient->IP, thisClient->port);
        n = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *)&cli_addr, &clilen);
        if (n < 0)
            printf("ERROR on recvfrom");
        printf("Received a request from %s:%d of +%s\n", thisClient->IP, thisClient->port, buf);
        thisClient->newRequestValue = atoi(buf);
    }

    close(sockfd);
}

// Returns the new client Index (ID)
CLIENT_INFO *AddNewClient(char* clientIP, int port)
{
    // char clientIP[INET_ADDRSTRLEN];
    // inet_ntop(AF_INET, &cli_addr.sin_addr, clientIP, INET_ADDRSTRLEN);
    clientsCount++;
    memcpy(&clients[clientsCount], NewClientStruct(clientsCount - 1, clientIP, port), sizeof(CLIENT_INFO));
    printf("Added new client: %s:%d\n", clientIP, port);

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
    memcpy(&copyOfClients, &clients, sizeof(copyOfClients));
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
    p->newRequestValue = 0;
    return p;
}