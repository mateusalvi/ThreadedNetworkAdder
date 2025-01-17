/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include "discovery.h"
#include "server_prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>

// Definição das variáveis globais
CLIENT_INFO clients[MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void SendMessage(char *message, char *ip, int port, char *returnMessage, bool expectReturn) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    static long long seqn = 1;

    printf("SendMessage: Preparing to send to %s:%d\n", ip, port);

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return;
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        printf("ERROR invalid IP address: %s\n", ip);
        close(sockfd);
        return;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;  // Converte para microssegundos
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting timeout");
        close(sockfd);
        return;
    }

    // Prepara o pacote
    packet request_packet;
    request_packet.type = REQ;
    request_packet.data.req.seqn = seqn++;
    request_packet.data.req.value = atoi(message);

    printf("SendMessage: Sending packet type=%d, seqn=%lld, value=%d\n", 
           request_packet.type, request_packet.data.req.seqn, request_packet.data.req.value);

    // Envia a mensagem
    if (sendto(sockfd, &request_packet, sizeof(request_packet), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR sending message");
        close(sockfd);
        return;
    }

    if (expectReturn) {
        // Recebe a resposta
        packet response_packet;
        int n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                        (struct sockaddr *)&server_addr, &server_len);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                switch (errno) {
                    case EBADF:
                        printf("ERROR EBADF\n");
                        break;
                    case ECONNREFUSED:
                        printf("ERROR ECONNREFUSED\n");
                        break;
                    case EFAULT:
                        printf("ERROR EFAULT\n");
                        break;
                    case EINTR:
                        printf("ERROR EINTR\n");
                        break;
                    case EINVAL:
                        printf("ERROR EINVAL\n");
                        break;
                    case ENOMEM:
                        printf("ERROR ENOMEM\n");
                        break;
                    case ENOTCONN:
                        printf("ERROR ENOTCONN\n");
                        break;
                    case ENOTSOCK:
                        printf("ERROR ENOTSOCK\n");
                        break;
                    default:
                        printf("ERROR NOT SPECIFIED recvfrom\n");
                        break;
                }
                perror("ERROR receiving response");
            }
            printf("SendMessage: No response received (timeout)\n");
        } else if (n != sizeof(response_packet)) {
            printf("SendMessage: Received incomplete packet: %d bytes\n", n);
        } else {
            printf("SendMessage: Received response packet type=%d, seqn=%lld, value=%d, status=%d\n", 
                   response_packet.type, response_packet.data.resp.seqn,
                   response_packet.data.resp.value, response_packet.data.resp.status);

            if (response_packet.type == REQ_ACK && 
                response_packet.data.resp.seqn == request_packet.data.req.seqn) {
                if (response_packet.data.resp.status == 0) {
                    snprintf(returnMessage, MAX_MESSAGE_LEN, "%d", response_packet.data.resp.value);
                } else {
                    printf("SendMessage: Server reported error: %d\n", response_packet.data.resp.status);
                }
            } else {
                printf("SendMessage: Received invalid response type=%d or seqn mismatch (got %lld, expected %lld)\n",
                       response_packet.type, response_packet.data.resp.seqn, request_packet.data.req.seqn);
            }
        }
    }

    close(sockfd);
}

void BroadcastSignIn(int port, char *returnMessage) {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    packet discovery_packet;
    static long long seqn = 1;

    printf("Starting server discovery on port %d...\n", port);

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return;
    }

    // Configura opções do socket para broadcast
    int broadcast_enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("ERROR setting broadcast option");
        close(sockfd);
        return;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;  // Converte para microssegundos
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting timeout");
        close(sockfd);
        return;
    }

    // Configura o endereço de broadcast
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(port);

    // Prepara o pacote de descoberta
    discovery_packet.type = DESC;
    discovery_packet.data.req.seqn = seqn++;
    discovery_packet.data.req.value = 0;

    printf("Sending discovery broadcast to port %d...\n", port);

    // Envia o pacote de descoberta
    if (sendto(sockfd, &discovery_packet, sizeof(discovery_packet), 0,
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("ERROR sending discovery packet");
        close(sockfd);
        return;
    }

    printf("Waiting for server response...\n");

    // Aguarda resposta
    packet response_packet;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    
    int n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                     (struct sockaddr *)&server_addr, &server_len);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            switch (errno) {
                case EBADF:
                    printf("ERROR EBADF\n");
                    break;
                case ECONNREFUSED:
                    printf("ERROR ECONNREFUSED\n");
                    break;
                case EFAULT:
                    printf("ERROR EFAULT\n");
                    break;
                case EINTR:
                    printf("ERROR EINTR\n");
                    break;
                case EINVAL:
                    printf("ERROR EINVAL\n");
                    break;
                case ENOMEM:
                    printf("ERROR ENOMEM\n");
                    break;
                case ENOTCONN:
                    printf("ERROR ENOTCONN\n");
                    break;
                case ENOTSOCK:
                    printf("ERROR ENOTSOCK\n");
                    break;
                default:
                    printf("ERROR NOT SPECIFIED recvfrom\n");
                    break;
            }
            perror("ERROR receiving broadcast response");
        }
        printf("No response received from server\n");
    } else {
        if (response_packet.type == DESC_ACK) {
            char server_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(server_addr.sin_addr), server_ip, INET_ADDRSTRLEN);
            printf("Received response from server at %s:%d\n", server_ip, response_packet.data.resp.value);
            
            // Formata a resposta como #IP#PORT
            snprintf(returnMessage, MAX_MESSAGE_LEN, "#%s#%d", server_ip, response_packet.data.resp.value);
        } else {
            printf("Received invalid response type: %d\n", response_packet.type);
        }
    }

    close(sockfd);
}

void ListenForNewClients(int port) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    packet received_packet, response_packet;

    printf("Starting client listener on port %d...\n", port);

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return;
    }

    // Configura opções do socket
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("ERROR setting socket options");
        close(sockfd);
        return;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;  // Converte para microssegundos
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting timeout");
        close(sockfd);
        return;
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Faz o bind do socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR on binding");
        close(sockfd);
        return;
    }

    printf("Client listener active on port %d...\n", port);

    while (1) {
        // Recebe a mensagem
        int n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0,
                        (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("ERROR receiving client message");
            }
        } else if (n != sizeof(received_packet)) {
            printf("Received incomplete packet: %d bytes\n", n);
        } else {
            printf("Received packet from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            if (received_packet.type == DESC) {
                // Prepara a resposta
                response_packet.type = DESC_ACK;
                response_packet.data.resp.seqn = received_packet.data.req.seqn;
                response_packet.data.resp.value = port + 1;  // Porta para comunicação futura
                response_packet.data.resp.status = 0;

                // Envia a resposta
                n = sendto(sockfd, &response_packet, sizeof(response_packet), 0,
                          (struct sockaddr *)&client_addr, sizeof(client_addr));
                if (n < 0) {
                    perror("ERROR sending response to client");
                } else if (n != sizeof(response_packet)) {
                    printf("Sent incomplete response: %d bytes\n", n);
                } else {
                    printf("Sent response to client at %s:%d\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                }
            }
        }
        usleep(100000);  // Evita consumo excessivo de CPU (100ms)
    }

    close(sockfd);
}

int ListenForAddRequest(int port, char *clientIP) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_MESSAGE_LEN];

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }

    // Configura opções do socket
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("ERROR setting socket options");
        close(sockfd);
        return -1;
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Faz o bind do socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR on binding");
        close(sockfd);
        return -1;
    }

    // Recebe mensagem do cliente
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                     (struct sockaddr *)&client_addr, &client_len);
    if (n < 0) {
        perror("ERROR receiving from client");
        close(sockfd);
        return -1;
    }

    buffer[n] = '\0';
    strcpy(clientIP, inet_ntoa(client_addr.sin_addr));

    // Converte a mensagem para número
    int value = atoi(buffer);

    // Envia confirmação
    char response[] = "OK";
    if (sendto(sockfd, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len) < 0) {
        perror("ERROR sending response to client");
    }

    close(sockfd);
    return value;
}

void addRequestListenerThread(void *arg) {
    CLIENT_INFO *client = (CLIENT_INFO *)arg;
    char *clientIP = client->ip;  // Agora usando ip em vez de IP
    int clientPort = client->port;
    char returnMessage[MAX_MESSAGE_LEN];

    printf("Starting request listener thread for client %s:%d\n", clientIP, clientPort);

    while (1) {
        int requestValue = ListenForAddRequest(clientPort, clientIP);
        if (requestValue >= 0) {
            printf("Received add request with value %d from %s:%d\n",
                   requestValue, clientIP, clientPort);
        }
        usleep(100000);  // 100ms sleep para evitar consumo excessivo de CPU
    }
}

CLIENT_INFO AddNewClient(char* clientIP, int port) {
    pthread_mutex_lock(&clients_mutex);
    CLIENT_INFO new_client = NewClientStruct(num_clients + 1, clientIP, port);
    if (num_clients < MAX_CLIENTS) {
        clients[num_clients++] = new_client;
    }
    pthread_mutex_unlock(&clients_mutex);
    return new_client;
}

CLIENT_INFO GetClientsVector() {
    // Retorna uma cópia do primeiro cliente (para compatibilidade)
    pthread_mutex_lock(&clients_mutex);
    CLIENT_INFO client = num_clients > 0 ? clients[0] : NewClientStruct(0, "", 0);
    pthread_mutex_unlock(&clients_mutex);
    return client;
}

CLIENT_INFO NewClientStruct(int id, char *ip, int port) {
    CLIENT_INFO client;
    client.id = id;
    strncpy(client.ip, ip, sizeof(client.ip) - 1);  // Agora usando ip em vez de IP
    client.ip[sizeof(client.ip) - 1] = '\0';
    client.port = port;
    return client;
}

char GetBroadcastAdress() {
    // Esta função não é mais necessária no Linux
    return '0';
}

// Testa a conexão com o servidor
bool TestServerConnection(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    packet test_packet, response_packet;
    static long long seqn = 1;

    printf("Testing connection to server at %s:%d\n", ip, port);

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return false;
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        printf("ERROR invalid IP address\n");
        close(sockfd);
        return false;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;  // Converte para microssegundos
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting timeout");
        close(sockfd);
        return false;
    }

    // Prepara o pacote de teste
    test_packet.type = REQ;
    test_packet.data.req.seqn = seqn++;
    test_packet.data.req.value = 0;

    printf("Sending test packet: type=%d, seqn=%lld, value=%d\n",
           test_packet.type, test_packet.data.req.seqn, test_packet.data.req.value);

    // Envia o pacote
    if (sendto(sockfd, &test_packet, sizeof(test_packet), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR sending test packet");
        close(sockfd);
        return false;
    }

    // Aguarda resposta
    int n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                     (struct sockaddr *)&server_addr, &server_len);
    
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("ERROR receiving test response");
        }
        printf("No response received from server (timeout)\n");
        close(sockfd);
        return false;
    }

    if (n != sizeof(response_packet)) {
        printf("Received incomplete response: %d bytes\n", n);
        close(sockfd);
        return false;
    }

    printf("Received response: type=%d, seqn=%lld, value=%d, status=%d\n",
           response_packet.type, response_packet.data.resp.seqn,
           response_packet.data.resp.value, response_packet.data.resp.status);

    close(sockfd);

    // Verifica se a resposta é válida
    return (response_packet.type == REQ_ACK &&
            response_packet.data.resp.seqn == test_packet.data.req.seqn &&
            response_packet.data.resp.status == 0);
}