/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

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
#include "server_prot.h"
#include "client.h"

#define BROADCAST_ADDR "255.255.255.255"
#define MAX_RETRIES 3
#define RETRY_DELAY_MS 1000

// Função para descobrir o servidor
int discover_server(int port, char* server_ip) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    static long long seqn = 1;

    printf("Starting server discovery on port %d...\n", port);

    // Cria o socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;  // Converte para microssegundos
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting timeout");
        close(sockfd);
        return -1;
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (server_ip != NULL) {
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("ERROR invalid server IP");
            close(sockfd);
            return -1;
        }
    } else {
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Usa localhost por padrão
    }

    // Prepara o pacote de descoberta
    packet discovery_packet;
    discovery_packet.type = DESC;
    discovery_packet.data.req.seqn = seqn++;
    discovery_packet.data.req.value = 0;

    printf("Sending discovery packet...\n");

    // Envia o pacote de descoberta
    if (sendto(sockfd, &discovery_packet, sizeof(discovery_packet), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR sending discovery packet");
        close(sockfd);
        return -1;
    }

    // Aguarda resposta
    packet response_packet;
    int n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                     (struct sockaddr *)&server_addr, &server_len);

    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("ERROR receiving discovery response");
        }
        printf("No response received from server (timeout)\n");
        close(sockfd);
        return -1;
    }

    if (n != sizeof(response_packet)) {
        printf("Received incomplete response: %d bytes\n", n);
        close(sockfd);
        return -1;
    }

    if (response_packet.type == DESC_ACK) {
        printf("Server found! Communication port: %d\n", response_packet.data.resp.value);
        close(sockfd);
        return response_packet.data.resp.value;
    } else {
        printf("Received invalid response type: %d\n", response_packet.type);
        close(sockfd);
        return -1;
    }
}

#include "client.h"

void* ClientInputSubprocess(void* arg) {
    // Esta thread agora só serve para capturar Ctrl+C
    while (1) {
        sleep(1);
    }
    return NULL;
}

void RunClient(int port) {
    int value = 0;
    int retries = 0;
    const int MAX_CLIENT_RETRIES = 3;
    static long long seqn = 1;
    char server_ip[INET_ADDRSTRLEN] = "127.0.0.1";  // Default to localhost
    int option;

    printf("\nChoose an option:\n");
    printf("1. Connect to localhost\n");
    printf("2. Connect to specific IP\n");
    printf("3. Exit\n");
    printf("Option: ");
    
    if (scanf("%d", &option) != 1) {
        printf("Invalid option\n");
        return;
    }

    // Limpa o buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    switch (option) {
        case 1:
            strncpy(server_ip, "127.0.0.1", sizeof(server_ip) - 1);
            break;
        case 2:
            printf("Enter server IP: ");
            if (fgets(server_ip, sizeof(server_ip), stdin) != NULL) {
                server_ip[strcspn(server_ip, "\n")] = 0;  // Remove newline
            }
            break;
        case 3:
            printf("Exiting...\n");
            return;
        default:
            printf("Invalid option\n");
            return;
    }

    // Thread para processar entrada do usuário
    pthread_t input_thread;
    if (pthread_create(&input_thread, NULL, ClientInputSubprocess, NULL) != 0) {
        perror("ERROR creating input thread");
        return;
    }

    // Loop principal do cliente
    while (1) {
        // Tenta descobrir o servidor
        int server_port = discover_server(port, server_ip);
        
        if (server_port > 0) {
            printf("Connected to server at %s:%d\n", server_ip, server_port);
            printf("Enter numbers to add (0 to exit):\n");

            // Loop para enviar valores
            while (1) {
                if (scanf("%d", &value) != 1) {
                    // Limpa o buffer de entrada
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF);
                    continue;
                }

                if (value == 0) {
                    break;
                }

                // Prepara o pacote de requisição
                packet request_packet;
                request_packet.type = REQ;
                request_packet.data.req.seqn = seqn++;
                request_packet.data.req.value = value;

                // Envia o pacote para o servidor
                int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                if (sockfd < 0) {
                    perror("ERROR opening socket");
                    continue;
                }

                // Configura timeout do socket
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;
                if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                    perror("ERROR setting timeout");
                    close(sockfd);
                    continue;
                }

                // Configura o endereço do servidor
                struct sockaddr_in req_addr;
                memset(&req_addr, 0, sizeof(req_addr));
                req_addr.sin_family = AF_INET;
                req_addr.sin_port = htons(server_port);
                if (inet_pton(AF_INET, server_ip, &req_addr.sin_addr) <= 0) {
                    perror("ERROR invalid server IP");
                    close(sockfd);
                    continue;
                }

                // Envia o pacote
                if (sendto(sockfd, &request_packet, sizeof(request_packet), 0,
                          (struct sockaddr *)&req_addr, sizeof(req_addr)) < 0) {
                    perror("ERROR sending request");
                    close(sockfd);
                    continue;
                }

                // Aguarda resposta
                packet response_packet;
                socklen_t addr_len = sizeof(req_addr);
                int n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                               (struct sockaddr *)&req_addr, &addr_len);

                if (n < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("ERROR receiving response");
                    }
                    printf("No response from server (timeout)\n");
                    close(sockfd);
                    break;
                }

                if (n != sizeof(response_packet)) {
                    printf("Received incomplete response: %d bytes\n", n);
                    close(sockfd);
                    break;
                }

                if (response_packet.type == REQ_ACK) {
                    printf("Current sum: %d\n", response_packet.data.resp.value);
                } else {
                    printf("Received invalid response type: %d\n", response_packet.type);
                    close(sockfd);
                    break;
                }

                close(sockfd);
            }
            break;  // Sai do loop principal se a conexão foi bem sucedida
        }

        if (++retries >= MAX_CLIENT_RETRIES) {
            printf("Failed to connect to server after %d attempts\n", MAX_CLIENT_RETRIES);
            break;
        }

        printf("Failed to connect to server. Retrying in 1 second...\n");
        sleep(1);
    }

    // Aguarda a thread de entrada terminar
    pthread_join(input_thread, NULL);
}