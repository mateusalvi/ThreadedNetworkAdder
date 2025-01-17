/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include "server_prot.h"
#include "discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

// Variáveis globais
static int total_sum = 0;
pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;

int receive_and_decode_message(int sockfd, packet *received_packet, struct sockaddr_in *client_addr) {
    socklen_t client_len = sizeof(struct sockaddr_in);

    // Recebe a mensagem
    int n = recvfrom(sockfd, received_packet, sizeof(packet), 0,
                     (struct sockaddr *)client_addr, &client_len);

    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("ERROR receiving message");
        }
        return -1;
    }

    if (n != sizeof(packet)) {
        printf("Received incomplete packet: %d bytes\n", n);
        return -1;
    }

    // Decodifica e valida o pacote
    printf("Received packet from %s:%d - ", 
           inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

    if (received_packet->type == REQ) {
        printf("type=REQ, seqn=%lld, value=%d\n",
               received_packet->data.req.seqn,
               received_packet->data.req.value);
    } else if (received_packet->type == DESC) {
        printf("type=DESC, seqn=%lld, value=%d\n",
               received_packet->data.req.seqn,
               received_packet->data.req.value);
    } else {
        printf("invalid type=%d\n", received_packet->type);
        return -1;
    }

    return n;
}

void discovery_service(int port) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    packet received_packet, response_packet;

    printf("Starting discovery service on port %d...\n", port);

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

    printf("Discovery service listening on port %d...\n", port);

    while (1) {
        // Recebe e decodifica a mensagem
        if (receive_and_decode_message(sockfd, &received_packet, &client_addr) >= 0) {
            printf("Discovery service: Processing packet from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            if (received_packet.type == DESC) {
                printf("Discovery service: Received DESC packet\n");

                // Prepara a resposta
                response_packet.type = DESC_ACK;
                response_packet.data.resp.seqn = received_packet.data.req.seqn;
                response_packet.data.resp.value = port + 1;  // Retorna a porta do serviço de requisições
                response_packet.data.resp.status = 0;    // Sucesso

                printf("Discovery service: Sending DESC_ACK with request port %d\n", port + 1);

                // Envia a resposta
                int n = sendto(sockfd, &response_packet, sizeof(response_packet), 0,
                             (struct sockaddr *)&client_addr, sizeof(client_addr));
                if (n < 0) {
                    perror("ERROR sending discovery response");
                } else if (n != sizeof(response_packet)) {
                    printf("Discovery service: Sent incomplete packet: %d bytes\n", n);
                } else {
                    printf("Discovery service: Response sent successfully\n");
                }
            } else {
                printf("Discovery service: Received invalid packet type: %d\n", received_packet.type);
            }
        }
        usleep(100000);  // Evita consumo excessivo de CPU (100ms)
    }

    close(sockfd);
}

void request_service(int port) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    packet received_packet;

    printf("Starting request service on port %d...\n", port);

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

    printf("Request service listening on port %d...\n", port);

    while (1) {
        // Recebe e decodifica a mensagem
        if (receive_and_decode_message(sockfd, &received_packet, &client_addr) >= 0) {
            printf("Request service: Processing packet from %s:%d\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Processa a mensagem recebida
            if (received_packet.type == REQ) {
                printf("Request service: Received REQ packet with seqn=%lld, value=%d\n",
                       received_packet.data.req.seqn, received_packet.data.req.value);

                // Se é um pacote de teste (valor 0), apenas responde com ACK
                if (received_packet.data.req.value == 0) {
                    packet response_packet;
                    response_packet.type = REQ_ACK;
                    response_packet.data.resp.seqn = received_packet.data.req.seqn;
                    response_packet.data.resp.value = 0;
                    response_packet.data.resp.status = 0;

                    printf("Request service: Sending test response\n");

                    int n = sendto(sockfd, &response_packet, sizeof(response_packet), 0,
                                 (struct sockaddr *)&client_addr, sizeof(client_addr));
                    if (n < 0) {
                        perror("ERROR sending test response");
                    } else if (n != sizeof(response_packet)) {
                        printf("Request service: Sent incomplete test response: %d bytes\n", n);
                    } else {
                        printf("Request service: Test response sent successfully\n");
                    }
                } else {
                    // Atualiza a soma total
                    pthread_mutex_lock(&sum_mutex);
                    total_sum += received_packet.data.req.value;
                    pthread_mutex_unlock(&sum_mutex);

                    // Envia resposta com a soma atual
                    packet response_packet;
                    response_packet.type = REQ_ACK;
                    response_packet.data.resp.seqn = received_packet.data.req.seqn;
                    response_packet.data.resp.value = total_sum;
                    response_packet.data.resp.status = 0;

                    printf("Request service: Sending REQ_ACK with total_sum %d\n", total_sum);

                    int n = sendto(sockfd, &response_packet, sizeof(response_packet), 0,
                                 (struct sockaddr *)&client_addr, sizeof(client_addr));
                    if (n < 0) {
                        perror("ERROR sending response");
                    } else if (n != sizeof(response_packet)) {
                        printf("Request service: Sent incomplete packet: %d bytes\n", n);
                    } else {
                        printf("Request service: Response sent successfully\n");
                    }
                }
            } else {
                printf("Request service: Received invalid packet type: %d\n", received_packet.type);
            }
        }
        usleep(100000);  // Evita consumo excessivo de CPU (100ms)
    }

    close(sockfd);
}

void *discovery_thread(void *arg) {
    discovery_service(*((int *)arg));
    return NULL;
}

void *request_thread(void *arg) {
    request_service(*((int *)arg));
    return NULL;
}

void start_server(int port) {
    pthread_t disc_thread, req_thread;
    int disc_port = port;
    int req_port = port + 1;

    // Inicia o serviço de descoberta em uma thread
    if (pthread_create(&disc_thread, NULL, discovery_thread, &disc_port) != 0) {
        perror("ERROR creating discovery thread");
        return;
    }

    // Inicia o serviço de requisições em outra thread
    if (pthread_create(&req_thread, NULL, request_thread, &req_port) != 0) {
        perror("ERROR creating request thread");
        return;
    }

    // Aguarda as threads terminarem (não deve acontecer normalmente)
    pthread_join(disc_thread, NULL);
    pthread_join(req_thread, NULL);

    // Limpa os recursos
    pthread_mutex_destroy(&sum_mutex);
}

void ServerMain(const char* port) {
    int port_num = atoi(port);
    if (port_num <= 0) {
        fprintf(stderr, "Invalid port number\n");
        return;
    }
    start_server(port_num);
}