/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include "server_prot.h"
#include "discovery.h"
#include "replication.h"
#include <signal.h>
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
static int running = 1;

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
    packet received_packet;

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

            // Ignora pacotes que não são de descoberta
            if (received_packet.type != DESC) {
                printf("Discovery service: Ignoring non-discovery packet type: %d\n", received_packet.type);
                continue;
            }

            printf("Discovery service: Received DESC packet\n");

            // Prepara a resposta
            packet response_packet;
            response_packet.type = DESC_ACK;
            response_packet.data.resp.seqn = received_packet.data.req.seqn;
            response_packet.data.resp.value = port + 1;  // Retorna a porta de requisições
            response_packet.data.resp.status = 0;  // Sucesso

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
        }
        usleep(100000);  // Evita consumo excessivo de CPU (100ms)
    }

    close(sockfd);
}

void *request_service(void *arg) {
    int port = (int)(long)arg;
    int sockfd;
    struct sockaddr_in server_addr;
    packet received_packet;

    printf("Starting request service on port %d...\n", port);

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return NULL;
    }

    // Configura opções do socket
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("ERROR setting socket options");
        close(sockfd);
        return NULL;
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
        return NULL;
    }

    printf("Request service listening on port %d...\n", port);

    while (running) {
        // Prepara para receber
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        memset(&received_packet, 0, sizeof(received_packet));

        // Recebe a mensagem (bloqueante)
        ssize_t n = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0,
                            (struct sockaddr *)&client_addr, &client_len);

        if (n < 0) {
            if (errno != EINTR) {  // Ignora interrupções
                perror("ERROR receiving request");
            }
            continue;
        }

        if (n != sizeof(received_packet)) {
            printf("Received incomplete packet: %d bytes\n", (int)n);
            continue;
        }

        printf("Request service: Processing packet from %s:%d (type=%d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
               received_packet.type);

        // Processa a requisição
        packet response_packet;
        memset(&response_packet, 0, sizeof(response_packet));
        response_packet.type = REQ_ACK;
        response_packet.data.resp.seqn = received_packet.data.req.seqn;

        if (received_packet.type != REQ) {
            printf("Request service: Ignoring non-request packet type: %d\n", received_packet.type);
            continue;
        }

        // Verifica se somos o primário
        if (!is_primary()) {
            printf("Request service: Not primary, sending error response\n");
            response_packet.data.resp.value = get_current_sum();  // Retorna soma atual
            response_packet.data.resp.status = 1;  // Status de erro - não é primário
        } else {
            printf("Request service: Processing value %d (seqn=%lld)\n",
                   received_packet.data.req.value, received_packet.data.req.seqn);

            // Atualiza o estado
            int current_sum = get_current_sum();
            int new_sum = current_sum + received_packet.data.req.value;
            int update_success = (update_state(new_sum, received_packet.data.req.seqn) == 0);

            // Pega o valor atualizado após a replicação
            current_sum = get_current_sum();

            // Prepara resposta com o valor ATUAL
            response_packet.data.resp.value = current_sum;  // Sempre usa o valor atual
            response_packet.data.resp.status = update_success ? 0 : 2;

            printf("Request service: State update %s (old_sum=%d, new_sum=%d)\n",
                   update_success ? "successful" : "failed",
                   current_sum, new_sum);
        }

        // Envia a resposta (tenta algumas vezes)
        int max_retries = 3;
        int retry;
        for (retry = 0; retry < max_retries; retry++) {
            printf("Request service: Sending response (attempt %d) to %s:%d (sum=%d)\n",
                   retry + 1, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                   response_packet.data.resp.value);

            n = sendto(sockfd, &response_packet, sizeof(response_packet), 0,
                      (struct sockaddr *)&client_addr, client_len);

            if (n == sizeof(response_packet)) {
                printf("Request service: Response sent successfully\n");
                break;
            }

            if (n < 0) {
                perror("ERROR sending response");
            } else {
                printf("Request service: Sent incomplete packet: %d bytes\n", (int)n);
            }

            usleep(100000);  // Espera 100ms antes de tentar novamente
        }

        if (retry == max_retries) {
            printf("Request service: Failed to send response after %d attempts\n", max_retries);
        }
    }

    close(sockfd);
    return NULL;
}

void init_server(int port) {
    printf("Starting server on port %d...\n", port);
    
    // Inicia o gerenciador de replicação
    init_replication_manager(port, port == 2000);  // Porta 2000 é o primário
    
    // Inicia as threads de serviço
    pthread_t discovery_thread_id, request_thread_id;
    pthread_create(&discovery_thread_id, NULL, (void*)discovery_service, (void*)(long)port);
    pthread_create(&request_thread_id, NULL, request_service, (void*)(long)(port + 1));
    
    // Aguarda as threads terminarem
    pthread_join(discovery_thread_id, NULL);
    pthread_join(request_thread_id, NULL);
    
    // Finaliza o gerenciador de replicação
    stop_replication_manager();
}

void stop_server() {
    running = 0;
}

void handle_sigint(int sig) {
    exit(0);
}

void ServerMain(const char* port) {
    int port_num = atoi(port);
    struct sigaction sa;
    sigaction(SIGINT, &sa, NULL);
    if (port_num <= 0) {
        fprintf(stderr, "Invalid port number\n");
        return;
    }
    init_server(port_num);
}