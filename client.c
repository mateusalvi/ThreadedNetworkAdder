/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
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
#include <signal.h>
#include "client.h"
#include "server_prot.h"

#define BROADCAST_ADDR "255.255.255.255"

volatile sig_atomic_t stop = 0;

void handle_sigint(int sig) {
    exit(0);
}

void* ClientInputSubprocess(void* arg) {
    // Esta thread agora só serve para capturar Ctrl+C
    while (!stop) {
        sleep(1);
    }
    return NULL;
}

int discover_server(int port, struct sockaddr_in* server_addr) {
    int discovery_socket;
    struct sockaddr_in broadcast_addr;
    packet discovery_packet;
    
    // Cria o socket
    discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("ERROR opening socket");
        return -1;
    }
    
    // Configura o socket para permitir broadcast
    int broadcast_enable = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("ERROR setting broadcast option");
        close(discovery_socket);
        return -1;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 1;  // 1 segundo de timeout
    tv.tv_usec = 0;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting socket timeout");
        close(discovery_socket);
        return -1;
    }
    
    // Configura o endereço de broadcast
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    
    // Prepara o pacote de descoberta
    memset(&discovery_packet, 0, sizeof(discovery_packet));
    discovery_packet.type = DESC;
    discovery_packet.data.req.seqn = 1;
    discovery_packet.data.req.value = 0;
    
    // Tenta encontrar um servidor começando da porta inicial
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    int request_port = -1;
    
    // Tenta todas as portas possíveis usando PORT_STEP
    for (int port = BASE_PORT; port < BASE_PORT + (MAX_SERVERS * PORT_STEP); port += PORT_STEP) {
        printf("Sending discovery packet to port %d...\n", port);
        
        // Configura o endereço do servidor
        broadcast_addr.sin_port = htons(port);
        
        // Envia o pacote de descoberta
        if (sendto(discovery_socket, &discovery_packet, sizeof(discovery_packet), 0,
                  (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("ERROR sending discovery packet");
            continue;
        }
               
        // Espera a resposta com timeout
        packet response;
        int n = recvfrom(discovery_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&recv_addr, &addr_len);
                        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("No response from port %d (timeout)\n", port);
            } else {
                perror("ERROR receiving discovery response");
            }
            continue;
        }
        
        // Se recebeu resposta, verifica se é do tipo correto
        if (response.type == DESC_ACK) {
            request_port = response.data.resp.value;
            printf("Server found! Communication port: %d\n", request_port);
            // Configura o endereço do servidor para uso posterior
            memset(server_addr, 0, sizeof(*server_addr));
            server_addr->sin_family = AF_INET;
            server_addr->sin_port = htons(request_port);
            server_addr->sin_addr = recv_addr.sin_addr;
            break;
        } else {
            printf("Received invalid response type from port %d: %d\n", port, response.type);
        }
    }
    
    close(discovery_socket);
    return request_port;
}

// Função para enviar requisição e receber resposta
int send_request(int sockfd, struct sockaddr_in* req_addr, int value, long long* seqn) {
    int max_retries = 3;
    int retry;

    for (retry = 0; retry < max_retries; retry++) {
        // Prepara e envia o pacote
        packet request_packet;
        memset(&request_packet, 0, sizeof(request_packet));
        request_packet.type = REQ;
        request_packet.data.req.seqn = (*seqn)++;
        request_packet.data.req.value = value;

        printf("Sending request: value=%d, seqn=%lld to %s:%d\n", 
               value, request_packet.data.req.seqn,
               inet_ntoa(req_addr->sin_addr), ntohs(req_addr->sin_port));

        // Tenta enviar
        ssize_t n = sendto(sockfd, &request_packet, sizeof(request_packet), 0,
                          (struct sockaddr *)req_addr, sizeof(*req_addr));
        
        if (n != sizeof(request_packet)) {
            if (n < 0) {
                perror("ERROR sending request");
            } else {
                printf("Sent incomplete packet: %d bytes\n", (int)n);
            }
            usleep(100000);  // 100ms entre tentativas
            continue;
        }

        // Aguarda a resposta
        packet response_packet;
        socklen_t addr_len = sizeof(*req_addr);
        n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                    (struct sockaddr *)req_addr, &addr_len);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Server not responding (timeout)\n");
                // Se for timeout, indica que precisa procurar novo primário
                return -2;
            }
            perror("ERROR receiving response");
            usleep(100000);  // 100ms entre tentativas
            continue;
        }

        if (n != sizeof(response_packet)) {
            printf("Received incomplete response: %d bytes\n", (int)n);
            continue;
        }

        if (response_packet.type != REQ_ACK) {
            printf("Received invalid response type: %d\n", response_packet.type);
            continue;
        }

        // Se o status é 1, significa que o servidor não é mais o primário
        if (response_packet.data.resp.status == 1) {
            printf("Server is not primary anymore\n");
            return -2;  // Código especial para indicar que precisa procurar novo primário
        }

        printf("Received response: value=%d, seqn=%lld, status=%d\n", 
               response_packet.data.resp.value, response_packet.data.resp.seqn,
               response_packet.data.resp.status);

        return response_packet.data.resp.value;
    }

    // Se chegou aqui, é porque falhou todas as tentativas
    // Indica que precisa procurar novo primário
    return -2;
}

void RunClient(int port) {
    int value = 0;
    int retries = 0;
    const int MAX_CLIENT_RETRIES = 3;
    static long long seqn = 1;
    struct sockaddr_in server_addr;

    // Configura o manipulador de sinal para SIGINT
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Thread para processar entrada do usuário
    pthread_t input_thread;
    if (pthread_create(&input_thread, NULL, ClientInputSubprocess, NULL) != 0) {
        perror("ERROR creating input thread");
        return;
    }

    // Loop principal do cliente
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return;
    }

    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 1;  // 1 segundo de timeout
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR setting socket timeout");
        close(sockfd);
        return;
    }

    while (!stop) {
        int option;
        printf("\nChoose an option:\n");
        printf("1. Broadcast to discover server\n");
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
                // Tenta descobrir o servidor usando broadcast
                server_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
                break;
            case 2:
                printf("Enter server IP: ");
                char server_ip[INET_ADDRSTRLEN];
                if (fgets(server_ip, sizeof(server_ip), stdin) != NULL) {
                    server_ip[strcspn(server_ip, "\n")] = 0;  // Remove newline
                    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
                        perror("ERROR invalid server IP");
                        continue;
                    }
                }
                break;
            case 3:
                printf("Exiting...\n");
                stop = 1;
                break;
            default:
                printf("Invalid option\n");
                continue;
        }

        if (stop) break;

        // Tenta descobrir o servidor
        int server_port = discover_server(port, &server_addr);
        
        if (server_port > 0) {
            printf("Connected to server at %s:%d\n", inet_ntoa(server_addr.sin_addr), server_port);
            server_addr.sin_port = htons(server_port);
            
            while (!stop) {
                printf("\nChoose an option:\n");
                printf("1. Send individual requests\n");
                printf("2. Read from file\n");
                printf("3. Exit\n");
                printf("Option: ");
                
                if (scanf("%d", &option) != 1) {
                    printf("Invalid option\n");
                    continue;
                }

                // Limpa o buffer
                while ((c = getchar()) != '\n' && c != EOF);

                switch (option) {
                    case 1:
                        printf("Enter numbers to add (0 to exit):\n");
                        // Loop para enviar valores individualmente
                        while (!stop) {
                            if (scanf("%d", &value) != 1) {
                                // Limpa o buffer de entrada
                                while ((c = getchar()) != '\n' && c != EOF);
                                continue;
                            }

                            if (value == 0) {
                                break;
                            }

                            int result = send_request(sockfd, &server_addr, value, &seqn);
                            if (result == -2) {
                                // Servidor não é mais primário, tenta descobrir novo primário
                                printf("Server is not primary anymore. Searching for new primary...\n");
                                server_port = discover_server(port, &server_addr);
                                if (server_port > 0) {
                                    printf("Found new primary at %s:%d\n", inet_ntoa(server_addr.sin_addr), server_port);
                                    server_addr.sin_port = htons(server_port);
                                    // Tenta enviar a requisição novamente
                                    result = send_request(sockfd, &server_addr, value, &seqn);
                                } else {
                                    printf("Could not find new primary server\n");
                                    break;
                                }
                            }
                            
                            if (result < 0) {
                                printf("Failed to send request\n");
                                break;
                            }

                            printf("Current sum: %d\n", result);
                        }
                        break;

                    case 2:
                        printf("Enter filename: ");
                        char filename[256];
                        if (fgets(filename, sizeof(filename), stdin) == NULL) {
                            printf("Error reading filename\n");
                            continue;
                        }
                        filename[strcspn(filename, "\n")] = 0;  // Remove newline

                        FILE *file = fopen(filename, "r");
                        if (file == NULL) {
                            perror("ERROR opening file");
                            continue;
                        }

                        while (fscanf(file, "%d", &value) == 1 && !stop) {
                            int result = send_request(sockfd, &server_addr, value, &seqn);
                            if (result == -2) {
                                // Servidor não é mais primário, tenta descobrir novo primário
                                printf("Server is not primary anymore. Searching for new primary...\n");
                                server_port = discover_server(port, &server_addr);
                                if (server_port > 0) {
                                    printf("Found new primary at %s:%d\n", inet_ntoa(server_addr.sin_addr), server_port);
                                    server_addr.sin_port = htons(server_port);
                                    // Tenta enviar a requisição novamente
                                    result = send_request(sockfd, &server_addr, value, &seqn);
                                } else {
                                    printf("Could not find new primary server\n");
                                    break;
                                }
                            }
                            
                            if (result < 0) {
                                printf("Failed to send request\n");
                                break;
                            }

                            printf("Current sum: %d\n", result);
                        }

                        fclose(file);
                        break;

                    case 3:
                        stop = 1;
                        break;

                    default:
                        printf("Invalid option\n");
                        break;
                }
            }
        } else {
            printf("Could not find server\n");
        }
    }

    close(sockfd);
    pthread_join(input_thread, NULL);
}