/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#define _GNU_SOURCE /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include "discovery.h"

int currentClients = 0;

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
    return broadcast_addr;
}

void SendMessage(char *message, char *ip, int port)
{
    printf("Sending package to %s:%d with message \"%s\"...\n", ip, port, message);

    int sockfd;
    struct sockaddr_in dest_addr;

    // Create socket for sending datagrams
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("Socket creation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Prepare the destination address structure
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;            // IPv4
    dest_addr.sin_port = htons(port);          // Port number
    dest_addr.sin_addr.s_addr = inet_addr(ip); // IP address of the destination (localhost in this case)

    // Prepare the data to send
    // char message[MAX_MESSAGE_LEN] = "SLEEPING";
    //char message[MAX_MESSAGE_LEN];
    //strcpy(message, message);
    int message_len = strlen(message);

    // Send the message
    if (sendto(sockfd, message, message_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1)
    {
        perror("Sendto failed!\n");
        exit(EXIT_FAILURE);
    }

    printf("Message sent successfully.\n");
    
    close(sockfd);
}

char* RecieveDiscovery(char* port)
{
    int sockfd;
    struct sockaddr_in my_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[MAX_MESSAGE_LEN];
    char* clientIP;

    // Create socket for receiving datagrams
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Prepare the server address structure
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;         // IPv4
    my_addr.sin_port = htons(atoi(port));       // Port number
    my_addr.sin_addr.s_addr = INADDR_ANY; // Accept packets from any IP address

    // Bind socket to the server address
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //TODO: THIS WILL KEEP LOOPING FOR EVER.
    while(1)
    {
        // Receive message from client
        client_addr_len = sizeof(client_addr);

        //------------------Aqui o programa vai ficar "parado" esperando a mensagem de alguem.
        printf("UDP server listening on port %d...\n", atoi(port));
        int recv_len = recvfrom(sockfd, buffer, MAX_MESSAGE_LEN, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_len == -1)
        {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
        }

        // Print details of the client
        char client_ip[INET_ADDRSTRLEN];
        //strcpy(clientIP, inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN));
        printf("Received packet from %s:%d\n", inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN), ntohs(client_addr.sin_port));



        // Print received message
        buffer[recv_len] = '\0'; // Add null terminator to received data
        
        if(strcmp(buffer, SERVER_DISCOVERY_MESSAGE) == 0)
        {
            printf("New client deected!\n");
            //TODO 
            CLIENT_INFO* newClient = (CLIENT_INFO*){ atoi(inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN)), 0, NULL, 1, 0 };// = (CLIENT_INFO*)NewClientStruct(atoi(inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN)));
            printf("HERE SHOULD GO THE ADD NEW CLIENT CODE");
            //memcpy(&newClient, (CLIENT_INFO*)NewClientStruct(atoi(inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN))), sizeof(CLIENT_INFO));
            AddNewClient(newClient);
            printf("New client added!\n");
        }

        printf(" with message: \"%s\"\n", buffer);
    }
    
    close(sockfd); // Close the socket

    return clientIP;
}

void AddNewClient(CLIENT_INFO *newClient)
{
    CLIENT_INFO* tempClients[MAX_CLIENTS];
    memcpy(&tempClients, GetClientsVector(), sizeof(CLIENT_INFO)*MAX_CLIENTS);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if(tempClients[i] != NULL && newClient->client_id == tempClients[i]->client_id)
        {
            return;
        }
    }
	//lock clientListMutex
	pthread_mutex_lock(&mutexClientList);
	//add new client to list
	//clients[currentClients] = *newClient;
    memcpy(&clients[currentClients], &newClient, sizeof(CLIENT_INFO)*MAX_CLIENTS);
	currentClients++;
	//unlock clientListMutex
	pthread_mutex_unlock(&mutexClientList);
}

CLIENT_INFO* GetClientsVector()
{
    CLIENT_INFO* copyOfClients[MAX_CLIENTS];
    pthread_mutex_lock(&mutexClientList);
    memcpy(&copyOfClients, &clients, sizeof(CLIENT_INFO)*MAX_CLIENTS);
    pthread_mutex_unlock(&mutexClientList);
    return *copyOfClients;
}

CLIENT_INFO* NewClientStruct(int id) { 
	CLIENT_INFO* p = malloc(sizeof(CLIENT_INFO));
	p->client_id = id;
	p->last_value = 0;
	return p;
}