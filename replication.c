#include "replication.h"
#include "discovery.h"
#include "config.h"
#include <stdarg.h>

// Níveis de log
typedef enum {
    LOG_INFO = 1,
    LOG_WARN,
    LOG_ERROR
} log_level;

// Gerenciador de replicação global
static replication_manager rm;

// Socket de replicação
static int replication_socket;

// Flag para controle das threads
static volatile int running = 1;

// Protótipos de funções estáticas
static void* replication_receiver_service(void* arg);
static void* primary_check_service(void* arg);
static void process_replication_message(replica_message* msg, struct sockaddr_in* sender_addr);
static void add_replica(int replica_id);
static void send_replica_list(int target_id);
static void start_election(void);
static void handle_election_start(replica_message* msg, struct sockaddr_in* sender_addr);
static void handle_election_response(replica_message* msg);
static void handle_victory_declaration(replica_message* msg, struct sockaddr_in* sender_addr);
static void handle_state_update(replica_message* msg, struct sockaddr_in* sender_addr);
static void check_primary_status(void);
static void log_message(log_level level, const char* format, ...);
static int send_join_request(void);

// Função de log
static void log_message(log_level level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    switch(level) {
        case LOG_INFO: fprintf(stderr, "[INFO] "); break;
        case LOG_WARN: fprintf(stderr, "[WARN] "); break;
        case LOG_ERROR: fprintf(stderr, "[ERROR] "); break;
    }
    
    vfprintf(stderr, format, args);
    va_end(args);
}

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
            handle_state_update(msg, sender_addr);
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
            // Atualiza lista de réplicas
            for (int i = 0; i < msg->replica_count; i++) {
                int found = 0;
                for (int j = 0; j < rm.replica_count; j++) {
                    if (rm.replicas[j].id == msg->replicas[i].id) {
                        rm.replicas[j] = msg->replicas[i];
                        found = 1;
                        break;
                    }
                }
                if (!found && rm.replica_count < MAX_REPLICAS) {
                    rm.replicas[rm.replica_count++] = msg->replicas[i];
                }
            }
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

// Adiciona uma nova réplica descoberta via broadcast
void add_discovered_replica(const char* ip, int port) {
    pthread_mutex_lock(&rm.state_mutex);
    
    // Procura se a réplica já existe
    int found = 0;
    for (int i = 0; i < rm.replica_count; i++) {
        struct sockaddr_in* addr = &rm.replicas[i].addr;
        if (addr->sin_addr.s_addr == inet_addr(ip) &&
            ntohs(addr->sin_port) == port) {
            // Atualiza timestamp
            rm.replicas[i].last_heartbeat = time(NULL);
            rm.replicas[i].is_alive = 1;
            found = 1;
            break;
        }
    }
    
    // Se não encontrou e há espaço, adiciona
    if (!found && rm.replica_count < MAX_REPLICAS) {
        // Configura endereço
        struct sockaddr_in* addr = &rm.replicas[rm.replica_count].addr;
        memset(addr, 0, sizeof(*addr));
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        addr->sin_addr.s_addr = inet_addr(ip);
        
        // Configura outros campos
        rm.replicas[rm.replica_count].id = rm.replica_count + 2000;  // IDs começam em 2000
        rm.replicas[rm.replica_count].last_heartbeat = time(NULL);
        rm.replicas[rm.replica_count].is_alive = 1;
        rm.replicas[rm.replica_count].state_confirmed = 0;
        
        rm.replica_count++;
        
        log_message(LOG_INFO, "Added new replica %s:%d with ID %d\n", 
                  ip, port, rm.replicas[rm.replica_count-1].id);
        
        // Se não estamos em eleição e não somos primário, inicia eleição
        if (!rm.is_primary && !rm.election_in_progress) {
            pthread_mutex_unlock(&rm.state_mutex);
            start_election();
            return;
        }
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
    pthread_mutex_lock(&rm.state_mutex);
    
    log_message(LOG_INFO, "Received election start from replica %d (my_id=%d)\n", 
              msg->replica_id, rm.my_id);
    
    // Se somos primário ou temos ID maior, respondemos
    if (rm.is_primary || msg->replica_id < rm.my_id) {
        log_message(LOG_INFO, "Responding to election as %s\n", 
                  rm.is_primary ? "current primary" : "higher ID");
        
        // Envia resposta
        replica_message response;
        memset(&response, 0, sizeof(response));
        response.type = ELECTION_RESPONSE;
        response.replica_id = rm.my_id;
        response.timestamp = time(NULL);
        
        // Envia resposta várias vezes para garantir entrega
        for (int i = 0; i < 3; i++) {
            sendto(replication_socket, &response, sizeof(response), MSG_CONFIRM,
                   (struct sockaddr*)sender_addr, sizeof(*sender_addr));
            usleep(10000); // 10ms entre tentativas
        }
        
        // Se não somos primário mas temos ID maior, iniciamos nossa eleição
        if (!rm.is_primary && msg->replica_id < rm.my_id) {
            pthread_mutex_unlock(&rm.state_mutex);
            start_election();
            return;
        }
    } else {
        log_message(LOG_INFO, "Ignoring election from higher ID %d (my_id=%d)\n",
                  msg->replica_id, rm.my_id);
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

static void handle_election_response(replica_message* msg) {
    pthread_mutex_lock(&rm.state_mutex);
    
    if (!rm.election_in_progress) {
        pthread_mutex_unlock(&rm.state_mutex);
        return;
    }
    
    if (msg->replica_id > rm.my_id) {
        log_message(LOG_INFO, "Received election response from higher ID %d, stopping election\n", msg->replica_id);
        // Se recebemos resposta de um ID maior, paramos nossa eleição
        rm.election_in_progress = 0;
        
        // Atualiza o status da réplica que respondeu
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id == msg->replica_id) {
                rm.replicas[i].is_alive = 1;
                rm.replicas[i].last_heartbeat = time(NULL);
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

static void handle_victory_declaration(replica_message* msg, struct sockaddr_in* sender_addr) {
    pthread_mutex_lock(&rm.state_mutex);
    
    log_message(LOG_INFO, "Received victory declaration from %d (my_id=%d)\n", 
              msg->replica_id, rm.my_id);
    
    // Se recebemos vitória de um ID maior, sempre aceitamos
    if (msg->replica_id > rm.my_id) {
        if (rm.is_primary) {
            log_message(LOG_INFO, "Stepping down from primary role for higher ID %d\n", msg->replica_id);
            rm.is_primary = 0;
        }
        
        log_message(LOG_INFO, "Accepting victory from %d\n", msg->replica_id);
        
        // Atualiza informações do novo primário
        rm.primary_id = msg->replica_id;
        rm.election_in_progress = 0;
        rm.received_initial_state = 0;  // Força receber novo estado
        
        // Atualiza estado apenas se o número de sequência for maior
        if (msg->last_seqn >= rm.last_seqn) {
            int old_sum = rm.current_sum;
            rm.current_sum = msg->current_sum;
            rm.last_seqn = msg->last_seqn;
            log_message(LOG_INFO, "Updated state from new primary: old_sum=%d, new_sum=%d, seqn=%lld\n",
                      old_sum, msg->current_sum, msg->last_seqn);
        } else {
            log_message(LOG_INFO, "Keeping current state (seqn %lld) as it's newer than primary's (seqn %lld)\n",
                      rm.last_seqn, msg->last_seqn);
        }
        
        // Atualiza informações do novo primário na lista de réplicas
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id == msg->replica_id) {
                rm.replicas[i].is_alive = 1;
                rm.replicas[i].last_heartbeat = time(NULL);
                rm.replicas[i].addr = *sender_addr;
                break;
            }
        }
        
        // Envia confirmação com estado atual
        replica_message ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = VICTORY_ACK;
        ack.replica_id = rm.my_id;
        ack.timestamp = time(NULL);
        ack.current_sum = rm.current_sum;  // Envia estado atual
        ack.last_seqn = rm.last_seqn;
        
        // Envia ACK várias vezes para garantir entrega
        for (int i = 0; i < 3; i++) {
            sendto(replication_socket, &ack, sizeof(ack), MSG_CONFIRM,
                   (struct sockaddr*)sender_addr, sizeof(*sender_addr));
            usleep(10000); // 10ms entre tentativas
        }
        
        log_message(LOG_INFO, "Sent VICTORY_ACK to new primary %d with state: sum=%d, seqn=%lld\n",
                  msg->replica_id, rm.current_sum, rm.last_seqn);
    } else {
        // Se recebemos vitória de um ID menor e somos primário, ignoramos
        log_message(LOG_INFO, "Ignoring victory declaration from lower ID %d (my_id=%d)\n", 
                  msg->replica_id, rm.my_id);
    }
    
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
    
    // Se já recebemos um state update recente do primário, não inicia eleição
    time_t now = time(NULL);
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == rm.primary_id && 
            rm.replicas[i].is_alive && 
            (now - rm.replicas[i].last_heartbeat) <= REPLICA_TIMEOUT) {
            log_message(LOG_INFO, "Primary %d is still alive, skipping election\n", rm.primary_id);
            pthread_mutex_unlock(&rm.state_mutex);
            return;
        }
    }
    
    rm.election_in_progress = 1;
    
    // Verifica réplicas com ID maior
    int has_higher_id = 0;
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id > rm.my_id) {
            has_higher_id = 1;
            log_message(LOG_INFO, "Sending election start to %d\n", rm.replicas[i].id);
            
            // Envia START_ELECTION
            replica_message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = START_ELECTION;
            msg.replica_id = rm.my_id;
            msg.timestamp = time(NULL);
            
            // Envia 3 vezes para garantir recebimento
            for (int j = 0; j < 3; j++) {
                sendto(replication_socket, &msg, sizeof(msg), MSG_CONFIRM,
                       (struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                usleep(10000); // 10ms entre tentativas
            }
        }
    }
    
    if (!has_higher_id) {
        // Se não há réplicas com ID maior, torna-se primário imediatamente
        rm.is_primary = 1;
        rm.primary_id = rm.my_id;
        rm.election_in_progress = 0;
        
        log_message(LOG_INFO, "No higher ID found in replica list, declaring victory\n");
        
        // Envia mensagem de vitória
        replica_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = VICTORY;
        msg.replica_id = rm.my_id;
        msg.timestamp = time(NULL);
        msg.current_sum = rm.current_sum;
        msg.last_seqn = rm.last_seqn;
        
        // Envia para todas as réplicas
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.my_id) {
                log_message(LOG_INFO, "Sending victory to %d\n", rm.replicas[i].id);
                for (int j = 0; j < 3; j++) {
                    sendto(replication_socket, &msg, sizeof(msg), MSG_CONFIRM,
                           (struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                    usleep(10000);
                }
            }
        }
        
        // Envia STATE_UPDATE inicial
        msg.type = STATE_UPDATE;
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.my_id) {
                log_message(LOG_INFO, "Sending initial state update to replica %d: sum=%d, seqn=%lld\n",
                          rm.replicas[i].id, rm.current_sum, rm.last_seqn);
                sendto(replication_socket, &msg, sizeof(msg), MSG_CONFIRM,
                       (struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
            }
        }
    } else {
        log_message(LOG_INFO, "Higher IDs found in replica list, waiting for their response\n");
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
    
    // Aguarda por ELECTION_TIMEOUT_MS antes de tentar novamente
    usleep(ELECTION_TIMEOUT_MS * 1000);
}

// Atualiza o estado do servidor
static void handle_state_update(replica_message* msg, struct sockaddr_in* sender_addr) {
    pthread_mutex_lock(&rm.state_mutex);
    
    // Verifica se a mensagem veio do primário atual
    if (msg->replica_id != rm.primary_id) {
        log_message(LOG_INFO, "Ignoring state update from non-primary %d\n", msg->replica_id);
        pthread_mutex_unlock(&rm.state_mutex);
        return;
    }
    
    // Atualiza estado apenas se o número de sequência for maior
    if (msg->last_seqn >= rm.last_seqn) {
        int old_sum = rm.current_sum;
        rm.current_sum = msg->current_sum;
        rm.last_seqn = msg->last_seqn;
        rm.election_in_progress = 0;  // Confirma fim da eleição ao receber state update
        
        log_message(LOG_INFO, "Updated state from primary: old_sum=%d, new_sum=%d, seqn=%lld\n",
                  old_sum, msg->current_sum, msg->last_seqn);
        
        // Marca que recebemos o estado inicial após eleição
        rm.received_initial_state = 1;
    } else {
        log_message(LOG_INFO, "Ignoring outdated state update (seqn %lld < current %lld)\n",
                  msg->last_seqn, rm.last_seqn);
    }
    
    // Envia ACK para o primário
    replica_message ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = STATE_ACK;
    ack.replica_id = rm.my_id;
    ack.timestamp = time(NULL);
    ack.current_sum = rm.current_sum;
    ack.last_seqn = rm.last_seqn;
    
    sendto(replication_socket, &ack, sizeof(ack), MSG_CONFIRM,
           (struct sockaddr*)sender_addr, sizeof(*sender_addr));
    
    log_message(LOG_INFO, "Sent STATE_ACK to primary %d: sum=%d, seqn=%lld\n",
              rm.primary_id, rm.current_sum, rm.last_seqn);
    
    pthread_mutex_unlock(&rm.state_mutex);
}
