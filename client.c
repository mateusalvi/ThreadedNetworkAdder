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
#include <sys/time.h>
#include <signal.h>
#include "client.h"
#include "server_prot.h"
#include "config.h"

// Constantes locais
#define RETRY_DELAY_MS 1000

// Protótipos das funções
void manual_input(struct sockaddr_in* server_addr, int client_socket);
void file_input(struct sockaddr_in* server_addr, int client_socket);
int discover_server(int start_port, struct sockaddr_in* server_addr);

// Encontra um servidor ativo
int discover_server(int start_port, struct sockaddr_in* server_addr) {
    int discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("Failed to create discovery socket");
        return -1;
    }
    
    // Configura timeout para o recvfrom
    struct timeval tv;
    tv.tv_sec = 1;  // 1 segundo de timeout
    tv.tv_usec = 0;
    setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Prepara o pacote de descoberta
    packet discovery_packet;
    discovery_packet.type = DESC;
    discovery_packet.data.req.seqn = 0;
    discovery_packet.data.req.value = 0;
    
    // Tenta encontrar um servidor começando da porta inicial
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    int request_port = -1;
    
    // Tenta todas as portas possíveis usando PORT_STEP
    for (int port = BASE_PORT; port < BASE_PORT + (MAX_SERVERS * PORT_STEP); port += PORT_STEP) {
        printf("Sending discovery packet to port %d...\n", port);
        
        // Configura o endereço do servidor
        server_addr->sin_family = AF_INET;
        server_addr->sin_port = htons(port);
        server_addr->sin_addr.s_addr = inet_addr("127.0.0.1");
        
        // Envia o pacote de descoberta
        sendto(discovery_socket, &discovery_packet, sizeof(discovery_packet), 0,
               (struct sockaddr*)server_addr, sizeof(*server_addr));
               
        // Espera a resposta
        packet response;
        int n = recvfrom(discovery_socket, &response, sizeof(response), 0,
                        (struct sockaddr*)&recv_addr, &addr_len);
                        
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("ERROR receiving discovery response");
            }
            printf("No response from port %d\n", port);
            continue;
        }
        
        // Se recebeu resposta, verifica se é do tipo correto
        if (response.type == DESC_ACK) {
            request_port = response.data.resp.value;
            printf("Server found! Communication port: %d\n", request_port);
            break;
        }
    }
    
    close(discovery_socket);
    return request_port;
}

// Função para enviar requisição e receber resposta
int send_request(int sockfd, struct sockaddr_in* req_addr, int value, long long* seqn) {
    // Prepara e envia o pacote
    packet request_packet;
    memset(&request_packet, 0, sizeof(request_packet));
    request_packet.type = REQ;
    request_packet.data.req.seqn = (*seqn)++;
    request_packet.data.req.value = value;

    printf("Sending request: value=%d, seqn=%lld to %s:%d\n", 
           value, request_packet.data.req.seqn,
           inet_ntoa(req_addr->sin_addr), ntohs(req_addr->sin_port));

    // Tenta enviar algumas vezes
    int max_retries = 3;
    int retry;
    for (retry = 0; retry < max_retries; retry++) {
        ssize_t n = sendto(sockfd, &request_packet, sizeof(request_packet), 0,
                          (struct sockaddr *)req_addr, sizeof(*req_addr));
        
        if (n == sizeof(request_packet)) {
            break;  // Envio bem sucedido
        }

        if (n < 0) {
            perror("ERROR sending request");
        } else {
            printf("Sent incomplete packet: %d bytes\n", (int)n);
        }

        if (retry < max_retries - 1) {
            printf("Retrying send (attempt %d of %d)...\n", retry + 2, max_retries);
            usleep(100000);  // 100ms entre tentativas
        }
    }

    if (retry == max_retries) {
        printf("Failed to send request after %d attempts\n", max_retries);
        return -1;
    }

    // Aguarda resposta
    packet response_packet;
    memset(&response_packet, 0, sizeof(response_packet));
    socklen_t addr_len = sizeof(*req_addr);

    // Tenta receber algumas vezes
    for (retry = 0; retry < max_retries; retry++) {
        ssize_t n = recvfrom(sockfd, &response_packet, sizeof(response_packet), 0,
                            (struct sockaddr *)req_addr, &addr_len);

        if (n == sizeof(response_packet)) {
            if (response_packet.type == REQ_ACK) {
                if (response_packet.data.resp.status != 0) {
                    printf("Server error: status=%d\n", response_packet.data.resp.status);
                    if (response_packet.data.resp.status == 1) {
                        printf("Server is not primary, trying to rediscover...\n");
                        return -2;  // Código especial para tentar redescobrir
                    }
                    return -1;
                }
                printf("Current sum: %d\n", response_packet.data.resp.value);
                return response_packet.data.resp.value;
            } else {
                printf("Received invalid response type: %d\n", response_packet.type);
            }
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("ERROR receiving response");
            }
            printf("No response from server\n");
        } else {
            printf("Received incomplete response: %d bytes\n", (int)n);
        }

        if (retry < max_retries - 1) {
            printf("Retrying receive (attempt %d of %d)...\n", retry + 2, max_retries);
            usleep(500000);  // 500ms entre tentativas
        }
    }

    printf("Failed to receive response after %d attempts\n", max_retries);
    return -1;
}

// Função para ler números de um arquivo
int read_numbers_from_file(const char* filename, int sockfd, struct sockaddr_in* req_addr, long long* seqn) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("ERROR opening file");
        return -1;
    }

    int value;
    int count = 0;
    printf("Reading numbers from file...\n");

    while (fscanf(file, "%d", &value) == 1) {
        printf("Sending value: %d\n", value);

        // Tenta enviar até 3 vezes em caso de falha
        int retries = 0;
        int result;
        while ((result = send_request(sockfd, req_addr, value, seqn)) < 0 && retries < MAX_RETRIES) {
            printf("Failed to send request, retrying...\n");
            retries++;
            sleep(1);
        }

        if (result < 0) {
            printf("Failed to send request after 3 attempts\n");
            break;
        }

        count++;
        usleep(100000);  // 100ms
    }

    fclose(file);
    printf("Finished reading file. Processed %d numbers.\n", count);
    return count;
}

void* ClientInputSubprocess(void* arg) {
    // Esta thread agora só serve para capturar Ctrl+C
    while (1) {
        sleep(1);
    }
    return NULL;
}

void RunClient(int port) {
    int retries = 0;
    pthread_t input_thread;
    long long seqn = 0;  // Adicionando declaração do número de sequência
    
    // Inicia thread de entrada do usuário
    pthread_create(&input_thread, NULL, (void*(*)(void*))ClientInputSubprocess, NULL);
    
    while (1) {
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("ERROR opening socket");
            break;
        }
        
        // Configura timeout para o socket
        struct timeval tv;
        tv.tv_sec = 1;  // 1 segundo
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        
        printf("\nChoose connection type:\n");
        printf("1. Connect to localhost\n");
        printf("2. Connect to specific IP\n");
        printf("3. Exit\n");
        printf("Option: ");
        
        int input_option;
        scanf("%d", &input_option);
        
        if (input_option == 3) {
            close(sockfd);
            break;
        }
        
        if (input_option == 1) {
            printf("Starting server discovery on port %d...\n", port);
            int request_port = discover_server(port, &server_addr);
            if (request_port < 0) {
                printf("No server found!\n");
                close(sockfd);
                continue;
            }
            server_addr.sin_port = htons(request_port);
        } else if (input_option == 2) {
            char ip[256];
            printf("Enter server IP: ");
            scanf("%s", ip);
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr.s_addr = inet_addr(ip);
        }
        
        getchar();  // Consome newline
        
        printf("\nChoose input method:\n");
        printf("1. Type numbers manually\n");
        printf("2. Read numbers from file\n");
        printf("Option: ");
        scanf("%d", &input_option);
        getchar();  // Consome newline
        
        if (input_option == 1) {
            printf("Enter numbers to add (0 to exit):\n");
            
            while (1) {
                int value;
                scanf("%d", &value);
                getchar();  // Consome newline
                
                if (value == 0) break;
                
                int result = -1;
                retries = 0;
                
                while (retries < MAX_RETRIES && result < 0) {
                    result = send_request(sockfd, &server_addr, value, &seqn);
                    
                    if (result < 0) {
                        printf("Failed to send request, retrying...\n");
                        // Tenta descobrir um novo servidor
                        printf("Starting server discovery...\n");
                        int new_port = discover_server(BASE_PORT, &server_addr);
                        if (new_port > 0) {
                            server_addr.sin_port = htons(new_port);
                            printf("Server changed! Updating connection to port %d\n", new_port);
                        }
                        retries++;
                        sleep(1);
                    }
                }
                
                if (result < 0) {
                    printf("Failed to send request after %d attempts\n", MAX_RETRIES);
                    break;
                }
            }
        } else if (input_option == 2) {
            char filename[256];
            printf("Enter the filename: ");
            if (fgets(filename, sizeof(filename), stdin) != NULL) {
                filename[strcspn(filename, "\n")] = 0;  // Remove newline
                read_numbers_from_file(filename, sockfd, &server_addr, &seqn);
            }
        } else {
            printf("Invalid option\n");
        }
        
        close(sockfd);
        break;
    }
    
    // Aguarda a thread de entrada terminar
    pthread_join(input_thread, NULL);
}

void manual_input(struct sockaddr_in* server_addr, int client_socket) {
    printf("Enter numbers to add (0 to exit):\n");
    
    int number;
    long long seqn = 1;
    int retry_count = 0;
    
    while (1) {
        scanf("%d", &number);
        if (number == 0) break;
        
        // Prepara o pacote
        packet request;
        request.type = REQ;
        request.data.req.seqn = seqn++;
        request.data.req.value = number;
        
        retry_count = 0;
        while (retry_count < MAX_RETRIES) {
            // Envia o pacote
            sendto(client_socket, &request, sizeof(request), 0,
                   (struct sockaddr*)server_addr, sizeof(*server_addr));
            
            // Espera a resposta
            packet response;
            socklen_t addr_len = sizeof(*server_addr);
            int n = recvfrom(client_socket, &response, sizeof(response), 0,
                            (struct sockaddr*)server_addr, &addr_len);
            
            if (n < 0) {
                printf("No response from server (timeout)\n");
                printf("Failed to send request, retrying...\n");
                
                // Tenta descobrir um novo servidor
                printf("Starting server discovery on port 2000...\n");
                int new_port = discover_server(2000, server_addr);
                if (new_port > 0) {
                    server_addr->sin_port = htons(new_port);
                    printf("Server changed! Updating connection to port %d\n", new_port);
                }
                
                retry_count++;
                continue;
            }
            
            // Se recebeu resposta, imprime o resultado
            if (response.type == REQ_ACK) {
                printf("Current sum: %d\n", response.data.resp.value);
                break;
            }
        }
        
        if (retry_count == MAX_RETRIES) {
            printf("Failed to send request after %d attempts\n", MAX_RETRIES);
            break;
        }
    }
}

// Função para ler números de um arquivo
void file_input(struct sockaddr_in* server_addr, int client_socket) {
    char filename[256];
    printf("Enter the filename: ");
    scanf("%s", filename);
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("ERROR opening file");
        return;
    }

    int value;
    int count = 0;
    long long seqn = 1;
    printf("Reading numbers from file...\n");

    while (fscanf(file, "%d", &value) == 1) {
        printf("Sending value: %d\n", value);

        // Prepara o pacote
        packet request;
        request.type = REQ;
        request.data.req.seqn = seqn++;
        request.data.req.value = value;

        // Tenta enviar até 3 vezes em caso de falha
        int retries = 0;
        while (retries < MAX_RETRIES) {
            // Envia o pacote
            sendto(client_socket, &request, sizeof(request), 0,
                   (struct sockaddr*)server_addr, sizeof(*server_addr));
            
            // Espera a resposta
            packet response;
            socklen_t addr_len = sizeof(*server_addr);
            int n = recvfrom(client_socket, &response, sizeof(response), 0,
                            (struct sockaddr*)server_addr, &addr_len);
            
            if (n < 0) {
                printf("No response from server (timeout)\n");
                printf("Failed to send request, retrying...\n");
                
                // Tenta descobrir um novo servidor
                printf("Starting server discovery on port 2000...\n");
                int new_port = discover_server(2000, server_addr);
                if (new_port > 0) {
                    server_addr->sin_port = htons(new_port);
                    printf("Server changed! Updating connection to port %d\n", new_port);
                }
                
                retries++;
                continue;
            }
            
            // Se recebeu resposta, imprime o resultado
            if (response.type == REQ_ACK) {
                printf("Current sum: %d\n", response.data.resp.value);
                break;
            }
        }

        if (retries == MAX_RETRIES) {
            printf("Failed to send request after %d attempts\n", MAX_RETRIES);
            break;
        }

        count++;
        usleep(100000);  // 100ms
    }

    fclose(file);
    printf("Finished reading file. Processed %d numbers.\n", count);
}

void ClientMain(const char* port) {
    int option;
    printf("\nChoose connection type:\n");
    printf("1. Connect to localhost\n");
    printf("2. Connect to specific IP\n");
    printf("3. Exit\n");
    printf("Option: ");
    scanf("%d", &option);
    
    if (option == 3) {
        return;
    }
    
    // Cria o socket para comunicação
    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0) {
        perror("Failed to create socket");
        return;
    }
    
    // Configura o endereço do servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    if (option == 1) {
        printf("Starting server discovery on port 2000...\n");
        int request_port = discover_server(2000, &server_addr);
        if (request_port < 0) {
            printf("No server found\n");
            close(client_socket);
            return;
        }
        server_addr.sin_port = htons(request_port);
    } else {
        char ip[20];
        int port;
        printf("Enter server IP: ");
        scanf("%s", ip);
        printf("Enter server port: ");
        scanf("%d", &port);
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(ip);
    }
    
    printf("Connected to server at %s:%d\n", 
           inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    
    // Menu de opções
    printf("\nChoose input method:\n");
    printf("1. Type numbers manually\n");
    printf("2. Read numbers from file\n");
    printf("Option: ");
    scanf("%d", &option);
    
    if (option == 1) {
        manual_input(&server_addr, client_socket);
    } else if (option == 2) {
        file_input(&server_addr, client_socket);
    } else {
        printf("Invalid option\n");
    }
    
    close(client_socket);
}