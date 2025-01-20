#include "replication.h"
#include "config.h"
#include "discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>

// Protótipos de funções internas
static void* heartbeat_service(void* arg);
static void* primary_check_service(void* arg);
static void* replication_receiver_service(void* arg);
static void process_replication_message(replica_message* msg, struct sockaddr_in* sender_addr);
static void add_replica(int replica_id);
static void send_replica_list(int target_id);
static void update_replica_list(replica_message* msg);
static void start_election(void);
static void check_primary_status(void);
static void handle_election_start(replica_message* msg, struct sockaddr_in* sender_addr);
static void handle_election_response(replica_message* msg);
static void handle_victory_declaration(replica_message* msg, struct sockaddr_in* sender_addr);

// Níveis de log
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level;

// Função de log
static void log_message(log_level level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    switch(level) {
        case LOG_DEBUG: fprintf(stderr, "[DEBUG] "); break;
        case LOG_INFO:  fprintf(stderr, "[INFO] "); break;
        case LOG_WARN:  fprintf(stderr, "[WARN] "); break;
        case LOG_ERROR: fprintf(stderr, "[ERROR] "); break;
    }
    
    vfprintf(stderr, format, args);
    va_end(args);
}

// Variáveis globais
static replication_manager rm;
static int replication_socket;
static volatile int running = 1;

// Processa mensagem de replicação recebida
static void process_replication_message(replica_message* msg, struct sockaddr_in* sender_addr) {
    // Só loga mensagens que não são heartbeat
    if (msg->type != HEARTBEAT) {
        const char* type_str = "UNKNOWN";
        switch(msg->type) {
            case HEARTBEAT: type_str = "HEARTBEAT"; break;
            case STATE_UPDATE: type_str = "STATE_UPDATE"; break;
            case STATE_ACK: type_str = "STATE_ACK"; break;
            case REPLICA_LIST_UPDATE: type_str = "REPLICA_LIST_UPDATE"; break;
            case START_ELECTION: type_str = "START_ELECTION"; break;
            case ELECTION_RESPONSE: type_str = "ELECTION_RESPONSE"; break;
            case VICTORY: type_str = "VICTORY"; break;
            case VICTORY_ACK: type_str = "VICTORY_ACK"; break;
        }
        log_message(LOG_INFO, "Received %s from %d\n", type_str, msg->replica_id);
    }
    
    switch(msg->type) {
        case HEARTBEAT:
            // Atualiza timestamp do último heartbeat recebido
            pthread_mutex_lock(&rm.state_mutex);
            for(int i = 0; i < rm.replica_count; i++) {
                if(rm.replicas[i].id == msg->replica_id) {
                    rm.replicas[i].last_heartbeat = time(NULL);
                    rm.replicas[i].is_alive = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&rm.state_mutex);
            break;
            
        case JOIN_REQUEST:
            if(rm.is_primary) {
                // Adiciona nova réplica e envia lista atualizada
                add_replica(msg->replica_id);
                send_replica_list(msg->replica_id);
            }
            break;
            
        case STATE_UPDATE:
            if(!rm.is_primary) {
                // Atualiza estado local
                pthread_mutex_lock(&rm.state_mutex);
                int old_sum = rm.current_sum;
                rm.current_sum = msg->current_sum;
                rm.last_seqn = msg->last_seqn;
                rm.received_initial_state = 1;  // Marca que recebemos o estado inicial
                pthread_mutex_unlock(&rm.state_mutex);
                
                log_message(LOG_INFO, "Received state update from primary: old_sum=%d, new_sum=%d, seqn=%lld\n",
                          old_sum, msg->current_sum, msg->last_seqn);
                
                // Envia confirmação
                replica_message response;
                memset(&response, 0, sizeof(response));
                response.type = STATE_ACK;
                response.replica_id = rm.my_id;
                response.primary_id = rm.primary_id;
                response.last_seqn = msg->last_seqn;
                
                sendto(replication_socket, &response, sizeof(response), 0,
                       (struct sockaddr*)sender_addr, sizeof(*sender_addr));
                
                // Verifica se o primário está vivo, se não estiver e não houver eleição em andamento, inicia uma
                time_t now = time(NULL);
                pthread_mutex_lock(&rm.state_mutex);
                for(int i = 0; i < rm.replica_count; i++) {
                    if(rm.replicas[i].id == rm.primary_id) {
                        if(!rm.replicas[i].is_alive && !rm.election_in_progress) {
                            log_message(LOG_INFO, "Primary %d is down after state update, starting election\n", rm.primary_id);
                            start_election();
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&rm.state_mutex);
            }
            break;
            
        case STATE_ACK:
            if(rm.is_primary) {
                // Marca réplica como tendo confirmado o estado
                pthread_mutex_lock(&rm.state_mutex);
                for(int i = 0; i < rm.replica_count; i++) {
                    if(rm.replicas[i].id == msg->replica_id) {
                        rm.replicas[i].state_confirmed = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&rm.state_mutex);
            }
            break;
            
        case REPLICA_LIST_UPDATE:
            update_replica_list(msg);
            break;
            
        case START_ELECTION:
            handle_election_start(msg, sender_addr);
            break;
            
        case ELECTION_RESPONSE:
            handle_election_response(msg);
            break;
            
        case VICTORY:
            handle_victory_declaration(msg, sender_addr);
            break;
            
        case VICTORY_ACK:
            // Atualiza status da réplica que confirmou a vitória
            pthread_mutex_lock(&rm.state_mutex);
            for(int i = 0; i < rm.replica_count; i++) {
                if(rm.replicas[i].id == msg->replica_id) {
                    rm.replicas[i].state_confirmed = 1;
                    log_message(LOG_INFO, "Replica %d acknowledged victory\n", msg->replica_id);
                    break;
                }
            }
            pthread_mutex_unlock(&rm.state_mutex);
            break;
    }
}

// Thread que processa mensagens de replicação
static void* replication_thread(void* arg) {
    log_message(LOG_INFO, "Started replication message thread\n");
    
    while (1) {
        replica_message msg;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        
        // Recebe mensagem
        ssize_t n = recvfrom(replication_socket, &msg, sizeof(msg), 0,
                           (struct sockaddr*)&sender_addr, &addr_len);
        
        if (n == sizeof(msg)) {
            process_replication_message(&msg, &sender_addr);
        }
    }
    
    return NULL;
}

// Thread para enviar heartbeats periódicos
static void* heartbeat_service(void* arg) {
    log_message(LOG_INFO, "Starting heartbeat service...\n");
    
    while (running) {
        pthread_mutex_lock(&rm.state_mutex);
        
        if (rm.is_primary) {
            // Envia heartbeat para todas as réplicas
            replica_message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = HEARTBEAT;
            msg.replica_id = rm.my_id;
            msg.primary_id = rm.my_id;
            msg.current_sum = rm.current_sum;
            msg.last_seqn = rm.last_seqn;
            msg.timestamp = time(NULL);
            
            // Envia para todas as réplicas vivas
            for (int i = 0; i < rm.replica_count; i++) {
                if (rm.replicas[i].id != rm.my_id && rm.replicas[i].is_alive) {
                    sendto(replication_socket, &msg, sizeof(msg), 0,
                           (const struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                    
                    // Não loga heartbeats
                    // log_message(LOG_DEBUG, "Sent heartbeat to replica %d (sum=%d, seqn=%lld)\n",
                    //           rm.replicas[i].id, msg.current_sum, msg.last_seqn);
                }
            }
        }
        
        pthread_mutex_unlock(&rm.state_mutex);
        usleep(HEARTBEAT_INTERVAL * 1000);  // Converte para microssegundos
    }
    
    return NULL;
}

// Envia pedido de join para o primário
static int send_join_request(void) {
    log_message(LOG_INFO, "Sending join request to primary...\n");
    
    // Prepara endereço do primário
    struct sockaddr_in primary_addr;
    memset(&primary_addr, 0, sizeof(primary_addr));
    primary_addr.sin_family = AF_INET;
    primary_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    primary_addr.sin_port = htons(rm.primary_id + 2);  // Porta de replicação
    
    // Prepara mensagem de join
    replica_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = JOIN_REQUEST;
    msg.replica_id = rm.my_id;
    msg.primary_id = rm.primary_id;
    msg.timestamp = time(NULL);
    
    // Tenta enviar por até 10 segundos
    time_t start_time = time(NULL);
    int received_state = 0;
    
    while (time(NULL) - start_time < 10 && !received_state) {
        // Envia pedido
        sendto(replication_socket, &msg, sizeof(msg), 0,
               (const struct sockaddr*)&primary_addr, sizeof(primary_addr));
        
        log_message(LOG_INFO, "Sent join request to primary at %s:%d\n",
                   inet_ntoa(primary_addr.sin_addr), ntohs(primary_addr.sin_port));
        
        // Aguarda resposta por 1 segundo
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(replication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        // Tenta receber resposta
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        replica_message response;
        
        ssize_t n = recvfrom(replication_socket, &response, sizeof(response), 0,
                            (struct sockaddr*)&sender_addr, &addr_len);
        
        if (n == sizeof(response)) {
            if (response.type == STATE_UPDATE) {
                pthread_mutex_lock(&rm.state_mutex);
                rm.current_sum = response.current_sum;
                rm.last_seqn = response.last_seqn;
                rm.received_initial_state = 1;
                pthread_mutex_unlock(&rm.state_mutex);
                
                log_message(LOG_INFO, "Received initial state: sum=%d, seqn=%lld\n",
                          response.current_sum, response.last_seqn);
                
                received_state = 1;
            }
        }
        
        if (!received_state) {
            log_message(LOG_INFO, "No response from primary, retrying...\n");
            sleep(1);
        }
    }
    
    return received_state ? 0 : -1;
}

// Adiciona uma nova réplica
static void add_replica(int replica_id) {
    //printf("Adding replica %d to the cluster\n", replica_id);
    
    // Verifica se já existe
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == replica_id) {
            rm.replicas[i].is_alive = 1;
            rm.replicas[i].last_heartbeat = time(NULL);
            rm.replicas[i].state_confirmed = 0;  // Reset confirmação
            //printf("Replica %d already exists, updating status\n", replica_id);
            return;
        }
    } 
    
    // Se não existe e há espaço, adiciona
    if (rm.replica_count < MAX_REPLICAS) {
        rm.replicas[rm.replica_count].id = replica_id;
        rm.replicas[rm.replica_count].is_alive = 1;
        rm.replicas[rm.replica_count].last_heartbeat = time(NULL);
        rm.replicas[rm.replica_count].state_confirmed = 0;
        
        // Configura endereço
        memset(&rm.replicas[rm.replica_count].addr, 0, sizeof(struct sockaddr_in));
        rm.replicas[rm.replica_count].addr.sin_family = AF_INET;
        rm.replicas[rm.replica_count].addr.sin_port = htons(replica_id);
        rm.replicas[rm.replica_count].addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        rm.replica_count++;
        
        log_message(LOG_INFO, "Added new replica %d to cluster (total=%d)\n",
                  replica_id, rm.replica_count);
        
        // Se sou primário, envio a lista atualizada para todas as réplicas
        if (rm.is_primary) {
            // Envia para todas as réplicas
            for (int i = 0; i < rm.replica_count; i++) {
                if (rm.replicas[i].id != rm.my_id) {
                    log_message(LOG_INFO, "Sending updated replica list to %d\n", rm.replicas[i].id);
                    send_replica_list(rm.replicas[i].id);
                }
            }
        }
        
        // Se não sou primário e descobri uma nova réplica, envio JOIN_REQUEST
        if (!rm.is_primary) {
            replica_message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = JOIN_REQUEST;
            msg.replica_id = rm.my_id;
            msg.timestamp = time(NULL);
            
            // Envia para o primário
            struct sockaddr_in primary_addr;
            memset(&primary_addr, 0, sizeof(primary_addr));
            primary_addr.sin_family = AF_INET;
            primary_addr.sin_port = htons(rm.primary_id + REPL_PORT_OFFSET);
            primary_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            log_message(LOG_INFO, "Sending join request to primary at port %d\n", rm.primary_id);
            
            // Envia várias vezes para garantir entrega
            for (int j = 0; j < 3; j++) {
                sendto(replication_socket, &msg, sizeof(msg), 0,
                       (const struct sockaddr*)&primary_addr, sizeof(primary_addr));
                usleep(10000); // 10ms entre tentativas
            }
        }
    } else {
        log_message(LOG_ERROR, "Error: Maximum number of replicas reached\n");
    }
}

// Função para enviar lista de réplicas para um novo servidor
static void send_replica_list(int target_id) {
    replica_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = REPLICA_LIST_UPDATE;
    msg.replica_id = rm.my_id;
    msg.primary_id = rm.primary_id;
    msg.timestamp = time(NULL);
    
    // Copia lista de réplicas
    pthread_mutex_lock(&rm.state_mutex);
    msg.replica_count = rm.replica_count;
    memcpy(msg.replicas, rm.replicas, sizeof(replica_info) * rm.replica_count);
    
    // Procura endereço do alvo
    struct sockaddr_in target_addr;
    int found = 0;
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == target_id) {
            target_addr = rm.replicas[i].addr;
            found = 1;
            break;
        }
    }
    
    if (found) {
        // Envia várias vezes para garantir entrega
        for (int i = 0; i < 3; i++) {
            sendto(replication_socket, &msg, sizeof(msg), 0,
                   (const struct sockaddr*)&target_addr, sizeof(target_addr));
            usleep(10000); // 10ms entre tentativas
        }
        log_message(LOG_INFO, "Sent replica list to new server %d (count=%d)\n",
                  target_id, msg.replica_count);
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

// Função para atualizar lista de réplicas locais
static void update_replica_list(replica_message* msg) {
    pthread_mutex_lock(&rm.state_mutex);
    
    log_message(LOG_INFO, "Updating replica list from primary (count=%d)\n",
              msg->replica_count);
    
    // Atualiza lista de réplicas
    rm.replica_count = msg->replica_count;
    memcpy(rm.replicas, msg->replicas, sizeof(replica_info) * msg->replica_count);
    
    // Atualiza informações do primário
    rm.primary_id = msg->primary_id;
    
    // Log das réplicas recebidas
    for (int i = 0; i < rm.replica_count; i++) {
        log_message(LOG_INFO, "Replica %d: id=%d, alive=%d, is_primary=%d\n",
                  i, rm.replicas[i].id, rm.replicas[i].is_alive,
                  rm.replicas[i].id == rm.primary_id);
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

// Inicializa o gerenciador de replicação
void init_replication_manager(int port, int is_primary) {
    printf("Initializing replication manager on port %d (is_primary=%d)...\n",
           port, is_primary);
           
    // Inicializa estrutura
    memset(&rm, 0, sizeof(rm));
    rm.my_id = port;
    rm.is_primary = is_primary;
    rm.primary_id = is_primary ? port : 0;
    rm.replica_count = 0;
    rm.current_sum = 0;
    rm.last_seqn = 0;
    rm.received_initial_state = is_primary;  // Primário já tem estado inicial
    rm.election_in_progress = 0;
    running = 1;
    pthread_mutex_init(&rm.state_mutex, NULL);
    
    // Se for primário, adiciona a si mesmo na lista
    if (is_primary) {
        rm.replicas[rm.replica_count].id = port;
        rm.replicas[rm.replica_count].is_alive = 1;
        rm.replicas[rm.replica_count].last_heartbeat = time(NULL);
        rm.replicas[rm.replica_count].state_confirmed = 1;
        
        // Configura endereço
        memset(&rm.replicas[rm.replica_count].addr, 0, sizeof(struct sockaddr_in));
        rm.replicas[rm.replica_count].addr.sin_family = AF_INET;
        rm.replicas[rm.replica_count].addr.sin_port = htons(port + REPL_PORT_OFFSET);
        rm.replicas[rm.replica_count].addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        rm.replica_count++;
        log_message(LOG_INFO, "Primary added itself to replica list\n");
    }
    
    // Configura socket de replicação
    replication_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (replication_socket < 0) {
        perror("Error creating replication socket");
        exit(1);
    }
    
    // Configura timeout do socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms
    setsockopt(replication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Configura endereço local para replicação
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port + REPL_PORT_OFFSET);
    
    if (bind(replication_socket, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Error binding replication socket");
        exit(1);
    }
    
    printf("Replication service listening on port %d...\n", port + REPL_PORT_OFFSET);
    
    // Se não for primário, adiciona o primário à lista
    if (!is_primary) {
        // Adiciona servidor primário (porta 2000)
        rm.primary_id = PRIMARY_PORT;
        printf("Added primary to replica list (port=%d, repl_port=%d)\n",
               PRIMARY_PORT, PRIMARY_PORT + REPL_PORT_OFFSET);
        
        // Configura endereço do primário
        memset(&rm.replicas[rm.replica_count].addr, 0, sizeof(struct sockaddr_in));
        rm.replicas[rm.replica_count].addr.sin_family = AF_INET;
        rm.replicas[rm.replica_count].addr.sin_port = htons(PRIMARY_PORT + REPL_PORT_OFFSET);
        rm.replicas[rm.replica_count].addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        rm.replicas[rm.replica_count].id = PRIMARY_PORT;
        rm.replicas[rm.replica_count].is_alive = 1;
        rm.replicas[rm.replica_count].last_heartbeat = time(NULL);
        rm.replica_count++;
        
        // Envia JOIN_REQUEST para o primário
        replica_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = JOIN_REQUEST;
        msg.replica_id = rm.my_id;
        msg.timestamp = time(NULL);
        
        struct sockaddr_in primary_addr;
        memset(&primary_addr, 0, sizeof(primary_addr));
        primary_addr.sin_family = AF_INET;
        primary_addr.sin_port = htons(PRIMARY_PORT + REPL_PORT_OFFSET);
        primary_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        printf("Sending join request to primary...\n");
        
        // Envia várias vezes para garantir entrega
        for (int i = 0; i < 3; i++) {
            printf("Sent join request to primary at %s:%d\n",
                   inet_ntoa(primary_addr.sin_addr), ntohs(primary_addr.sin_port));
                   
            sendto(replication_socket, &msg, sizeof(msg), 0,
                   (const struct sockaddr*)&primary_addr, sizeof(primary_addr));
            usleep(10000);  // 10ms entre tentativas
        }
    }
    
    // Inicia thread de recebimento
    pthread_t receiver_thread;
    pthread_create(&receiver_thread, NULL, replication_receiver_service, NULL);
    pthread_detach(receiver_thread);
    
    // Inicia thread de verificação do primário se não for primário
    if (!is_primary) {
        pthread_t checker_thread;
        pthread_create(&checker_thread, NULL, primary_check_service, NULL);
        pthread_detach(checker_thread);
        log_message(LOG_INFO, "Started primary checker thread\n");
    }
    
    // Inicia thread de heartbeat se for primário
    if (is_primary) {
        pthread_t heartbeat_thread;
        pthread_create(&heartbeat_thread, NULL, heartbeat_service, NULL);
        pthread_detach(heartbeat_thread);
        log_message(LOG_INFO, "Started heartbeat thread\n");
    }
    
    printf("Starting replication listener service...\n");
}

// Serviço de recebimento de mensagens de replicação
static void* replication_receiver_service(void* arg) {
    replica_message msg;
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    while (1) {
        ssize_t recv_len = recvfrom(replication_socket, &msg, sizeof(msg), 0,
                                  (struct sockaddr*)&sender_addr, &addr_len);
        
        if (recv_len == sizeof(msg)) {
            // Atualiza endereço do remetente
            for (int i = 0; i < rm.replica_count; i++) {
                if (rm.replicas[i].id == msg.replica_id) {
                    rm.replicas[i].addr = sender_addr;
                    break;
                }
            }
            
            // Processa a mensagem
            process_replication_message(&msg, &sender_addr);
        }
    }
    
    return NULL;
}

// Para o gerenciador de replicação
void stop_replication_manager() {
    running = 0;  // Sinaliza threads para pararem
    log_message(LOG_INFO, "Stopping replication manager...\n");
    
    // Fecha o socket de replicação
    if (replication_socket >= 0) {
        log_message(LOG_INFO, "Closing replication socket...\n");
        close(replication_socket);
        replication_socket = -1;
    }
    
    pthread_mutex_destroy(&rm.state_mutex);
    log_message(LOG_INFO, "Replication manager stopped\n");
}

// Verifica se esta réplica é o primário
int is_primary() {
    return rm.is_primary;
}

// Retorna a soma atual do estado replicado
int get_current_sum() {
    int sum;
    pthread_mutex_lock(&rm.state_mutex);
    sum = rm.current_sum;
    log_message(LOG_INFO, "Getting current sum: %d\n", sum);
    pthread_mutex_unlock(&rm.state_mutex);
    return sum;
}

// Inicializa o serviço de replicação
void init_replication(int my_id, int primary_id) {
    log_message(LOG_INFO, "Initializing replication service (my_id=%d, primary=%d)\n",
                my_id, primary_id);
    
    // Inicializa estrutura de gerenciamento
    rm.my_id = my_id;
    rm.primary_id = primary_id;
    rm.is_primary = (my_id == primary_id);
    rm.replica_count = 0;
    rm.current_sum = 0;
    rm.last_seqn = 0;
    rm.election_in_progress = 0;
    
    // Cria socket UDP para comunicação entre réplicas
    replication_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (replication_socket < 0) {
        perror("Failed to create replication socket");
        exit(1);
    }
    
    // Configura endereço para bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(my_id);  // Usa ID como porta
    
    // Faz bind do socket
    if (bind(replication_socket, (const struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind replication socket");
        exit(1);
    }
    
    log_message(LOG_INFO, "Replication service bound to port %d\n", my_id);
    
    // Se não for o primário, envia JOIN_REQUEST para todas as portas possíveis
    if (!rm.is_primary) {
        // Envia JOIN_REQUEST para uma faixa de portas
        replica_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = JOIN_REQUEST;
        msg.replica_id = rm.my_id;
        msg.timestamp = time(NULL);
        
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        // Tenta portas de 2000 a 2040 (ajuste conforme necessário)
        for (int port = 2000; port <= 2040; port += 4) {
            if (port != rm.my_id) {  // Não envia para si mesmo
                dest_addr.sin_port = htons(port);
                // Envia 3 vezes para garantir
                for (int i = 0; i < 3; i++) {
                    sendto(replication_socket, &msg, sizeof(msg), 0,
                           (const struct sockaddr*)&dest_addr, sizeof(dest_addr));
                    log_message(LOG_INFO, "Sent JOIN_REQUEST to port %d\n", port);
                    usleep(10000);  // 10ms entre tentativas
                }
            }
        }
    }
    
    // Inicia thread para processar mensagens de replicação
    pthread_t msg_thread;
    pthread_create(&msg_thread, NULL, replication_thread, NULL);
    pthread_detach(msg_thread);
    
    // Se for primário, inicia serviço de heartbeat
    if (rm.is_primary) {
        pthread_t heartbeat_thread;
        pthread_create(&heartbeat_thread, NULL, heartbeat_service, NULL);
        pthread_detach(heartbeat_thread);
    }
}

// Funções de manipulação de eleição
static void handle_election_start(replica_message* msg, struct sockaddr_in* sender_addr) {
    if (!rm.is_primary) {  // Qualquer não-primário pode responder
        log_message(LOG_INFO, "Received election start from replica %d (my_id=%d)\n", 
                  msg->replica_id, rm.my_id);
        
        if (msg->replica_id < rm.my_id) {
            // PRIMEIRO: Responde ao remetente que tem ID maior
            replica_message response;
            memset(&response, 0, sizeof(response));
            response.type = ELECTION_RESPONSE;
            response.replica_id = rm.my_id;
            response.timestamp = time(NULL);
            
            // Envia resposta várias vezes para garantir entrega
            for (int i = 0; i < 3; i++) {
                sendto(replication_socket, &response, sizeof(response), 0,
                       (const struct sockaddr*)sender_addr, sizeof(*sender_addr));
                usleep(10000); // 10ms entre tentativas
            }
            
            log_message(LOG_INFO, "Sent election response to replica %d (my ID %d is higher)\n",
                      msg->replica_id, rm.my_id);
            
            // DEPOIS: Inicia sua própria eleição imediatamente
            start_election();
        } else {
            log_message(LOG_INFO, "Ignoring election from higher ID %d (my_id=%d)\n",
                      msg->replica_id, rm.my_id);
        }
    } else {
        log_message(LOG_INFO, "Ignoring election start, I am primary\n");
    }
}

static void handle_election_response(replica_message* msg) {
    pthread_mutex_lock(&rm.state_mutex);
    
    if (msg->replica_id > rm.my_id) {
        log_message(LOG_INFO, "Received response from higher ID %d, stepping down\n",
                  msg->replica_id);
        rm.election_in_progress = 0;  // Desiste da eleição
    } else {
        log_message(LOG_INFO, "Ignoring response from lower ID %d\n",
                  msg->replica_id);
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

static void handle_victory_declaration(replica_message* msg, struct sockaddr_in* sender_addr) {
    pthread_mutex_lock(&rm.state_mutex);
    
    // Só processa vitória se for de uma eleição em andamento ou se for do primário atual
    if (!rm.election_in_progress && msg->replica_id != rm.primary_id) {
        pthread_mutex_unlock(&rm.state_mutex);
        return;
    }
    
    log_message(LOG_INFO, "Received victory from %d (my_id=%d, was_primary=%d, in_election=%d)\n",
              msg->replica_id, rm.my_id, rm.is_primary, rm.election_in_progress);
    
    // Atualiza estado local
    rm.is_primary = 0;
    rm.primary_id = msg->replica_id;
    rm.election_in_progress = 0;
    
    // Atualiza o estado com o estado do novo primário
    int old_sum = rm.current_sum;
    rm.current_sum = msg->current_sum;
    rm.last_seqn = msg->last_seqn;
    
    log_message(LOG_INFO, "Received state update from primary: old_sum=%d, new_sum=%d, seqn=%lld\n",
              old_sum, msg->current_sum, msg->last_seqn);
    
    // Confirma recebimento da vitória
    replica_message ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = VICTORY_ACK;
    ack.replica_id = rm.my_id;
    ack.timestamp = time(NULL);
    
    // Envia confirmação uma única vez
    sendto(replication_socket, &ack, sizeof(ack), 0,
           (struct sockaddr*)sender_addr, sizeof(*sender_addr));
    
    log_message(LOG_INFO, "Sent victory ACK to new primary %d\n", msg->replica_id);
    
    pthread_mutex_unlock(&rm.state_mutex);
}

// Atualiza o estado do servidor
int update_state(int new_sum, long long seqn) {
    pthread_mutex_lock(&rm.state_mutex);
    
    // Atualiza estado local
    rm.current_sum = new_sum;
    rm.last_seqn = seqn;
    
    // Se sou primário, propaga atualização para réplicas
    if (rm.is_primary) {
        replica_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = STATE_UPDATE;
        msg.replica_id = rm.my_id;
        msg.current_sum = rm.current_sum;
        msg.last_seqn = rm.last_seqn;
        msg.timestamp = time(NULL);
        
        // Envia para todas as réplicas
        int updates_sent = 0;
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.my_id && rm.replicas[i].is_alive) {
                sendto(replication_socket, &msg, sizeof(msg), 0,
                       (struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                log_message(LOG_INFO, "Sent state update to replica %d: sum=%d, seqn=%lld\n",
                          rm.replicas[i].id, new_sum, seqn);
                updates_sent++;
            }
        }
        
        if (updates_sent == 0) {
            log_message(LOG_INFO, "No replicas to update\n");
        }
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
    return 0;
}

// Serviço de verificação do primário
static void* primary_check_service(void* arg) {
    log_message(LOG_INFO, "Primary check service started\n");
    while (running) {
        check_primary_status();
        usleep(1000000);  // Verifica a cada 1 segundo
    }
    return NULL;
}

// Verifica status do primário
static void check_primary_status(void) {
    if (rm.is_primary) return;  // Só réplicas verificam o primário
    
    time_t now = time(NULL);
    static time_t last_log = 0;
    int primary_found = 0;
    
    pthread_mutex_lock(&rm.state_mutex);
    
    // Procura o primário na lista
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == rm.primary_id) {
            primary_found = 1;
            time_t time_since_heartbeat = now - rm.replicas[i].last_heartbeat;
            
            // Verifica se o primário está vivo baseado no último heartbeat
            if (rm.replicas[i].is_alive && time_since_heartbeat <= PRIMARY_TIMEOUT) {
                // Loga status do primário a cada 5 segundos
                if (now - last_log >= 5) {
                    log_message(LOG_INFO, "Primary check: Primary %d is alive, last heartbeat %ld seconds ago\n", 
                              rm.primary_id, time_since_heartbeat);
                    last_log = now;
                }
            } else {
                // Se o primário não responde por PRIMARY_TIMEOUT segundos
                if (rm.received_initial_state) {
                    rm.replicas[i].is_alive = 0;  // Marca primário como morto
                    pthread_mutex_unlock(&rm.state_mutex);  // Libera mutex antes de iniciar eleição
                    
                    log_message(LOG_INFO, "Primary %d is down, starting election\n", rm.primary_id);
                    start_election();  // Inicia eleição quando o primário falha
                    return;  // Retorna pois já liberou o mutex
                } else {
                    log_message(LOG_INFO, "Primary %d is down but haven't received initial state\n", 
                              rm.primary_id);
                }
            }
            break;
        }
    }
    
    if (!primary_found) {
        pthread_mutex_unlock(&rm.state_mutex);  // Libera mutex antes de iniciar eleição
        log_message(LOG_INFO, "Primary check: Primary %d not found in replica list\n", rm.primary_id);
        start_election();
        return;  // Retorna pois já liberou o mutex
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

// Inicia uma eleição
static void start_election(void) {
    log_message(LOG_INFO, "Starting election process...\n");
    
    pthread_mutex_lock(&rm.state_mutex);
    rm.election_in_progress = 1;
    pthread_mutex_unlock(&rm.state_mutex);
    
    // Verifica réplicas vivas
    time_t now = time(NULL);
    int has_higher_alive = 0;
    
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].is_alive && 
            rm.replicas[i].id > rm.my_id &&
            (now - rm.replicas[i].last_heartbeat) <= REPLICA_TIMEOUT) {
            has_higher_alive = 1;
            break;
        }
    }
    
    if (!has_higher_alive) {
        pthread_mutex_lock(&rm.state_mutex);
        // Torna-se primário
        rm.is_primary = 1;
        rm.primary_id = rm.my_id;
        rm.election_in_progress = 0;  // Termina eleição
        pthread_mutex_unlock(&rm.state_mutex);
        
        log_message(LOG_INFO, "No higher ID alive, declaring victory\n");
        
        // Envia mensagem de vitória
        replica_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = VICTORY;
        msg.replica_id = rm.my_id;
        msg.timestamp = time(NULL);
        msg.current_sum = rm.current_sum;  // Envia estado atual
        msg.last_seqn = rm.last_seqn;
        
        // Envia para todas as réplicas
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.my_id && rm.replicas[i].is_alive) {
                log_message(LOG_INFO, "Sending victory to %d\n", rm.replicas[i].id);
                for (int j = 0; j < 3; j++) {  // Envia 3 vezes
                    sendto(replication_socket, &msg, sizeof(msg), 0,
                           (struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                    usleep(10000); // 10ms entre tentativas
                }
            }
        }
    } else {
        // Envia START_ELECTION para todas as réplicas com ID maior
        log_message(LOG_INFO, "Found higher ID alive, starting election process\n");
        
        replica_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = START_ELECTION;
        msg.replica_id = rm.my_id;
        msg.timestamp = time(NULL);
        
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id > rm.my_id && rm.replicas[i].is_alive) {
                log_message(LOG_INFO, "Sending election start to %d\n", rm.replicas[i].id);
                for (int j = 0; j < 3; j++) {  // Envia 3 vezes
                    sendto(replication_socket, &msg, sizeof(msg), 0,
                           (struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                    usleep(10000); // 10ms entre tentativas
                }
            }
        }
    }
}
