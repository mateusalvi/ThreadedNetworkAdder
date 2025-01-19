/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "discovery.h"
#include "server_prot.h"
#include "config.h"

// Estrutura para controle interno de clientes
typedef struct {
    struct sockaddr_in addr;
    time_t last_seen;
} INTERNAL_CLIENT_INFO;

// Array de clientes conectados
static INTERNAL_CLIENT_INFO clients[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Socket para descoberta
static int discovery_socket;

// Porta do serviço de requisições
static int request_port;

// Flag para controle de threads
static volatile int running = 1;

// Protótipos de funções estáticas
static void* discovery_service(void* arg);
static void handle_discovery_packet(struct sockaddr_in* client_addr);
static void cleanup_clients();

// Inicializa o serviço de descoberta
void init_discovery_service(int port, int req_port) {
    printf("Starting discovery service on port %d...\n", port);
    
    // Salva a porta do serviço de requisições
    request_port = req_port;
    
    // Inicializa array de clientes
    memset(clients, 0, sizeof(clients));
    client_count = 0;
    
    // Cria o socket
    discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("Failed to create discovery socket");
        exit(1);
    }
    
    // Configura o socket para reusar endereço
    int opt = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Failed to set socket options");
        exit(1);
    }
    
    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Failed to set socket timeout");
        exit(1);
    }
    
    // Configura endereço
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    // Faz o bind
    if (bind(discovery_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind discovery socket");
        exit(1);
    }
    
    printf("Discovery service listening on port %d...\n", port);
    
    // Inicia thread de descoberta
    pthread_t discovery_thread;
    pthread_create(&discovery_thread, NULL, discovery_service, NULL);
}

// Para o serviço de descoberta
void stop_discovery_service() {
    running = 0;
    close(discovery_socket);
    pthread_mutex_destroy(&clients_mutex);
}

// Thread principal do serviço de descoberta
static void* discovery_service(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[MAX_MESSAGE_LEN];
    
    while (running) {
        // Recebe pacote de descoberta
        ssize_t recv_len = recvfrom(discovery_socket, buffer, sizeof(buffer), 0,
                                  (struct sockaddr*)&client_addr, &addr_len);
                                  
        if (recv_len < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Failed to receive discovery packet");
            }
            continue;
        }
        
        // Processa pacote
        handle_discovery_packet(&client_addr);
        
        // Limpa clientes inativos
        cleanup_clients();
    }
    
    return NULL;
}

// Processa um pacote de descoberta
static void handle_discovery_packet(struct sockaddr_in* client_addr) {
    pthread_mutex_lock(&clients_mutex);
    
    // Procura cliente na lista
    int found = 0;
    for (int i = 0; i < client_count; i++) {
        if (clients[i].addr.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == client_addr->sin_port) {
            // Atualiza timestamp
            clients[i].last_seen = time(NULL);
            found = 1;
            break;
        }
    }
    
    // Se não encontrou e há espaço, adiciona
    if (!found && client_count < MAX_CLIENTS) {
        clients[client_count].addr = *client_addr;
        clients[client_count].last_seen = time(NULL);
        client_count++;
        
        printf("New client connected from %s:%d\n",
               inet_ntoa(client_addr->sin_addr),
               ntohs(client_addr->sin_port));
    }
    
    // Envia resposta com porta do serviço de requisições
    packet response;
    response.type = DESC_ACK;
    response.data.disc.port = request_port;
    
    sendto(discovery_socket, &response, sizeof(response), 0,
           (struct sockaddr*)client_addr, sizeof(*client_addr));
           
    pthread_mutex_unlock(&clients_mutex);
}

// Remove clientes inativos
static void cleanup_clients() {
    pthread_mutex_lock(&clients_mutex);
    
    time_t now = time(NULL);
    int i = 0;
    
    while (i < client_count) {
        if (now - clients[i].last_seen > DISCOVERY_TIMEOUT_MS/1000) {
            printf("Client timeout: %s:%d\n",
                   inet_ntoa(clients[i].addr.sin_addr),
                   ntohs(clients[i].addr.sin_port));
                   
            // Remove cliente movendo os outros para frente
            memmove(&clients[i], &clients[i + 1],
                    (client_count - i - 1) * sizeof(INTERNAL_CLIENT_INFO));
            client_count--;
        } else {
            i++;
        }
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// Função para criar nova estrutura de cliente
CLIENT_INFO NewClientStruct(int id, char *ip, int port) {
    CLIENT_INFO client;
    client.id = id;
    strncpy(client.ip, ip, INET_ADDRSTRLEN - 1);
    client.ip[INET_ADDRSTRLEN - 1] = '\0';
    client.port = port;
    return client;
}

// Thread para lidar com requisições de adição
void* addRequestListenerThread(void* arg) {
    // TODO: Implementar thread de requisições
    (void)arg;  // Evita warning de variável não usada
    return NULL;
}

// Thread para processar requisição de adição
void* addRequestThread(void* arg) {
    // TODO: Implementar processamento de requisições
    (void)arg;  // Evita warning de variável não usada
    return NULL;
}

// Função para adicionar novo cliente
void AddNewClient(char *clientIP, int port) {
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(clientIP);
        addr.sin_port = htons(port);
        
        clients[client_count].addr = addr;
        clients[client_count].last_seen = time(NULL);
        client_count++;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Função para obter vetor de clientes
CLIENT_INFO GetClientsVector() {
    pthread_mutex_lock(&clients_mutex);
    CLIENT_INFO client = client_count > 0 ? 
        NewClientStruct(1, inet_ntoa(clients[0].addr.sin_addr), ntohs(clients[0].addr.sin_port)) :
        NewClientStruct(0, "", 0);
    pthread_mutex_unlock(&clients_mutex);
    return client;
}

// Envia mensagem para um servidor
void SendMessage(char *message, char *ip, int port, char *returnMessage, bool expectReturn) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[MAX_MESSAGE_LEN];
    
    // Cria socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Failed to create socket");
        return;
    }
    
    // Configura timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Failed to set socket timeout");
        close(sockfd);
        return;
    }
    
    // Configura endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    
    // Envia mensagem
    if (sendto(sockfd, message, strlen(message), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to send message");
        close(sockfd);
        return;
    }
    
    // Se espera resposta, aguarda
    if (expectReturn) {
        socklen_t addr_len = sizeof(server_addr);
        ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&server_addr, &addr_len);
                                   
        if (recv_len < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Failed to receive response");
            }
            close(sockfd);
            return;
        }
        
        buffer[recv_len] = '\0';
        strcpy(returnMessage, buffer);
    }
    
    close(sockfd);
}