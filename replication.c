#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>  // Adicionado para va_list, va_start, va_end

#include "replication.h"
#include "config.h"

#define CHECK_INTERVAL 1  // Intervalo em segundos para verificar status do primário

// Níveis de log
typedef enum {
    LOG_ERROR = 1,   // Erros críticos
    LOG_WARN,        // Avisos importantes
    LOG_INFO,        // Informações gerais
    LOG_DEBUG        // Informações detalhadas para debug
} log_level;

// Nível de log atual
static log_level current_log_level = LOG_INFO;

// Função de log
static void log_message(log_level level, const char* format, ...) {
    if (level > current_log_level) return;
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}

// Forward declarations
static void* listener_service(void* arg);
static void* heartbeat_service(void* arg);
static void* primary_check_service(void* arg);
static int send_join_request(void);
static void check_primary(void);
static void process_replication_message(replica_message* msg, struct sockaddr_in* sender_addr);
static void add_replica(int id);

// Variáveis globais
static replication_manager rm;
static int replication_socket;
static volatile int running = 1;  // Flag para controlar threads

// Thread para receber mensagens de replicação
static void* listener_service(void* arg) {
    log_message(LOG_INFO, "Starting listener service...\n");
    
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    replica_message msg;
    
    while (running) {
        memset(&msg, 0, sizeof(msg));
        ssize_t recv_len = recvfrom(replication_socket, &msg, sizeof(msg), 0,
                                  (struct sockaddr*)&sender_addr, &addr_len);
        
        if (recv_len < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log_message(LOG_ERROR, "Error receiving message: %s\n", strerror(errno));
            }
            continue;
        }
        
        // Processa a mensagem recebida
        process_replication_message(&msg, &sender_addr);
        
        // Verifica se deve se tornar primário
        check_primary();
    }
    
    return NULL;
}

// Inicia uma eleição
static void start_election(void) {
    log_message(LOG_INFO, "Starting election process...\n");
    
    pthread_mutex_lock(&rm.state_mutex);
    
    // Se já estiver em eleição, ignora
    if (rm.election_in_progress == 1) {
        log_message(LOG_INFO, "Election already in progress, skipping\n");
        pthread_mutex_unlock(&rm.state_mutex);
        return;
    }
    rm.election_in_progress = 1;
    
    // Log do estado atual antes de começar
    log_message(LOG_INFO, "Current state before election:\n");
    log_message(LOG_INFO, "My ID: %d\n", rm.my_id);
    log_message(LOG_INFO, "Primary ID: %d\n", rm.primary_id);
    log_message(LOG_INFO, "Total replicas: %d\n", rm.replica_count);
    
    // Primeiro, conta quantos servidores estão vivos
    int alive_count = 0;
    log_message(LOG_INFO, "Checking alive replicas:\n");
    for (int i = 0; i < rm.replica_count; i++) {
        time_t now = time(NULL);
        int is_alive = rm.replicas[i].is_alive && 
                      (now - rm.replicas[i].last_heartbeat) < REPLICA_TIMEOUT;
        
        // Log detalhado de cada réplica
        log_message(LOG_INFO, "Replica %d: id=%d, alive=%d, last_heartbeat=%ld seconds ago, is_primary=%d, is_me=%d\n",
                  i,
                  rm.replicas[i].id,
                  is_alive,
                  now - rm.replicas[i].last_heartbeat,
                  rm.replicas[i].id == rm.primary_id,
                  rm.replicas[i].id == rm.my_id);
        
        // Conta todas as réplicas vivas exceto eu mesmo
        if (rm.replicas[i].id != rm.my_id && is_alive) {
            alive_count++;
            log_message(LOG_INFO, "Found alive replica %d\n", rm.replicas[i].id);
        }
    }
    
    log_message(LOG_INFO, "Found %d alive replicas\n", alive_count);
    
    // Envia mensagem de eleição para todos os processos com ID maior
    replica_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = START_ELECTION;
    msg.replica_id = rm.my_id;
    msg.timestamp = time(NULL);
    
    int higher_ids = 0;  // Conta quantos processos têm ID maior
    
    // Envia para todas as réplicas com ID maior
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id != rm.primary_id && // Não envia para o primário morto
            rm.replicas[i].id != rm.my_id && 
            rm.replicas[i].is_alive) {
            
            if (rm.replicas[i].id > rm.my_id) {
                higher_ids++;
                log_message(LOG_INFO, "Sending election message to higher ID replica %d\n", rm.replicas[i].id);
            } else {
                log_message(LOG_INFO, "Not sending election message to lower ID replica %d\n", rm.replicas[i].id);
            }
            
            sendto(replication_socket, &msg, sizeof(msg), 0,
                   (const struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
        }
    }
    
    // Se não há IDs maiores, vence automaticamente
    if (higher_ids == 0) {
        log_message(LOG_INFO, "No higher IDs found among %d alive replicas, winning election by default\n", 
                   alive_count);
        // Torna-se o novo primário
        rm.is_primary = 1;
        rm.primary_id = rm.my_id;
        rm.election_in_progress = 0;  // Termina eleição
        
        // Anuncia vitória
        msg.type = ELECTION_VICTORY;
        msg.replica_id = rm.my_id;
        msg.primary_id = rm.my_id;
        msg.current_sum = rm.current_sum;
        msg.last_seqn = rm.last_seqn;
        msg.timestamp = time(NULL);
        
        int victory_sent = 0;
        // Envia mensagem de vitória várias vezes para cada réplica
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.primary_id && // Não envia para o primário morto
                rm.replicas[i].id != rm.my_id && 
                rm.replicas[i].is_alive) {
                // Envia 3 vezes para cada réplica
                for (int j = 0; j < 3; j++) {
                    sendto(replication_socket, &msg, sizeof(msg), 0,
                           (const struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                    usleep(10000); // 10ms entre tentativas
                }
                victory_sent++;
                log_message(LOG_INFO, "Sent victory message to replica %d\n", rm.replicas[i].id);
            }
        }
        
        log_message(LOG_INFO, "Won election, sent victory message to %d replicas\n", victory_sent);
        
        // Aguarda confirmações por um tempo
        struct timeval tv;
        tv.tv_sec = 1;  // Espera 1 segundo por confirmações
        tv.tv_usec = 0;
        setsockopt(replication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        time_t start_time = time(NULL);
        int confirmations = 0;
        
        while (time(NULL) - start_time < 1) {
            replica_message response;
            struct sockaddr_in sender_addr;
            socklen_t addr_len = sizeof(sender_addr);
            
            ssize_t n = recvfrom(replication_socket, &response, sizeof(response), 0,
                               (struct sockaddr*)&sender_addr, &addr_len);
            
            if (n == sizeof(response) && response.type == STATE_ACK) {
                log_message(LOG_INFO, "Received victory confirmation from replica %d\n", 
                          response.replica_id);
                confirmations++;
            }
        }
        
        log_message(LOG_INFO, "Received %d confirmations out of %d replicas\n", 
                   confirmations, victory_sent);
        
        // Inicia thread de heartbeat
        pthread_t heartbeat_thread;
        pthread_create(&heartbeat_thread, NULL, heartbeat_service, NULL);
        pthread_detach(heartbeat_thread);
    } else {
        log_message(LOG_INFO, "Waiting for responses from %d higher ID replicas\n", higher_ids);
        
        // Aguarda resposta por um tempo
        struct timeval tv;
        tv.tv_sec = ELECTION_TIMEOUT_MS / 1000;
        tv.tv_usec = (ELECTION_TIMEOUT_MS % 1000) * 1000;
        setsockopt(replication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        time_t start_time = time(NULL);
        int received_response = 0;
        
        while (time(NULL) - start_time < ELECTION_TIMEOUT_MS/1000 && !received_response) {
            replica_message response;
            struct sockaddr_in sender_addr;
            socklen_t addr_len = sizeof(sender_addr);
            
            ssize_t n = recvfrom(replication_socket, &response, sizeof(response), 0,
                               (struct sockaddr*)&sender_addr, &addr_len);
            
            if (n == sizeof(response)) {
                if (response.type == ELECTION_RESPONSE) {
                    if (response.replica_id > rm.my_id) {
                        log_message(LOG_INFO, "Received election response from higher ID %d, stepping down\n",
                                  response.replica_id);
                        received_response = 1;
                        rm.election_in_progress = 0;  // Desiste da eleição
                    } else {
                        log_message(LOG_INFO, "Ignoring election response from lower ID %d\n",
                                  response.replica_id);
                    }
                } else if (response.type == ELECTION_VICTORY) {
                    log_message(LOG_INFO, "Received victory message from replica %d while waiting\n",
                              response.replica_id);
                    rm.primary_id = response.replica_id;
                    rm.current_sum = response.current_sum;
                    rm.last_seqn = response.last_seqn;
                    rm.is_primary = 0;  // Garante que não sou primário
                    rm.election_in_progress = 0;  // Termina eleição
                    received_response = 1;
                }
            }
        }
        
        // Se não recebeu resposta, vence a eleição
        if (!received_response) {
            log_message(LOG_INFO, "No response from %d higher IDs after %d ms, winning election\n",
                       higher_ids, ELECTION_TIMEOUT_MS);
            rm.is_primary = 1;
            rm.primary_id = rm.my_id;
            rm.election_in_progress = 0;  // Termina eleição
            
            // Anuncia vitória
            msg.type = ELECTION_VICTORY;
            msg.replica_id = rm.my_id;
            msg.primary_id = rm.my_id;
            msg.current_sum = rm.current_sum;
            msg.last_seqn = rm.last_seqn;
            msg.timestamp = time(NULL);
            
            int victory_sent = 0;
            // Envia mensagem de vitória várias vezes para cada réplica
            for (int i = 0; i < rm.replica_count; i++) {
                if (rm.replicas[i].id != rm.primary_id && // Não envia para o primário morto
                    rm.replicas[i].id != rm.my_id && 
                    rm.replicas[i].is_alive) {
                    // Envia 3 vezes para cada réplica
                    for (int j = 0; j < 3; j++) {
                        sendto(replication_socket, &msg, sizeof(msg), 0,
                               (const struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
                        usleep(10000); // 10ms entre tentativas
                    }
                    victory_sent++;
                    log_message(LOG_INFO, "Sent victory message to replica %d\n", rm.replicas[i].id);
                }
            }
            
            log_message(LOG_INFO, "Won election, sent victory message to %d replicas\n", victory_sent);
            
            // Aguarda confirmações por um tempo
            struct timeval tv;
            tv.tv_sec = 1;  // Espera 1 segundo por confirmações
            tv.tv_usec = 0;
            setsockopt(replication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            time_t start_time = time(NULL);
            int confirmations = 0;
            
            while (time(NULL) - start_time < 1) {
                replica_message response;
                struct sockaddr_in sender_addr;
                socklen_t addr_len = sizeof(sender_addr);
                
                ssize_t n = recvfrom(replication_socket, &response, sizeof(response), 0,
                                   (struct sockaddr*)&sender_addr, &addr_len);
                
                if (n == sizeof(response) && response.type == STATE_ACK) {
                    log_message(LOG_INFO, "Received victory confirmation from replica %d\n", 
                              response.replica_id);
                    confirmations++;
                }
            }
            
            log_message(LOG_INFO, "Received %d confirmations out of %d replicas\n", 
                       confirmations, victory_sent);
            
            // Inicia thread de heartbeat
            pthread_t heartbeat_thread;
            pthread_create(&heartbeat_thread, NULL, heartbeat_service, NULL);
            pthread_detach(heartbeat_thread);
        }
    }
    
    // Se ainda estiver em eleição (algo deu errado), reseta
    if (rm.election_in_progress) {
        log_message(LOG_INFO, "Election got stuck, resetting election state\n");
        rm.election_in_progress = 0;
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
}

// Verifica se um servidor deve se tornar primário
static void check_primary(void) {
    if (rm.is_primary) {
        return;  // Se já sou primário, não preciso verificar
    }

    pthread_mutex_lock(&rm.state_mutex);
    time_t now = time(NULL);

    // Procura o primário na lista de réplicas
    int primary_alive = 0;
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == rm.primary_id) {
            time_t time_diff = now - rm.replicas[i].last_heartbeat;
            
            // Primário está vivo se recebemos heartbeat recentemente
            if (rm.replicas[i].is_alive && time_diff < PRIMARY_TIMEOUT) {
                primary_alive = 1;
                break;
            }
        }
    }

    // Se o primário está morto
    if (!primary_alive) {
        log_message(LOG_INFO, "Primary %d appears to be down (no heartbeat)\n", rm.primary_id);
        
        // Aguarda um tempo proporcional ao ID antes de iniciar a eleição
        // Isso dá prioridade para IDs menores e evita eleições simultâneas
        usleep((rm.my_id % 3) * 500000);  // Espera 0, 0.5 ou 1 segundo dependendo do ID
        
        // Verifica novamente se alguém já se tornou primário enquanto esperávamos
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.primary_id && 
                rm.replicas[i].id != rm.my_id &&
                rm.replicas[i].is_alive && 
                now - rm.replicas[i].last_heartbeat < REPLICA_TIMEOUT) {
                pthread_mutex_unlock(&rm.state_mutex);
                return;  // Outra réplica pode ter se tornado primária
            }
        }
        
        // Se ninguém se tornou primário, inicia a eleição
        pthread_mutex_unlock(&rm.state_mutex);
        start_election();
        return;
    }

    pthread_mutex_unlock(&rm.state_mutex);
}

// Processa uma mensagem de replicação
static void process_replication_message(replica_message* msg, struct sockaddr_in* sender_addr) {
    // Log da mensagem recebida com detalhes do tipo
    const char* type_str;
    switch (msg->type) {
        case START_ELECTION: type_str = "START_ELECTION"; break;
        case ELECTION_RESPONSE: type_str = "ELECTION_RESPONSE"; break;
        case ELECTION_VICTORY: type_str = "ELECTION_VICTORY"; break;
        case STATE_UPDATE: type_str = "STATE_UPDATE"; break;
        case STATE_ACK: type_str = "STATE_ACK"; break;
        case JOIN_REQUEST: type_str = "JOIN_REQUEST"; break;
        case HEARTBEAT: type_str = "HEARTBEAT"; break;
        default: type_str = "UNKNOWN"; break;
    }
    
    // Não loga heartbeats para reduzir ruído
    if (msg->type != HEARTBEAT) {
        log_message(LOG_INFO, "Processing message type=%s (%d) from replica %d (my_id=%d)\n", 
                    type_str, msg->type, msg->replica_id, rm.my_id);
    }
    
    pthread_mutex_lock(&rm.state_mutex);
    
    // Adiciona remetente à lista de réplicas se necessário
    add_replica(msg->replica_id);
    
    // Atualiza endereço e status da réplica
    int replica_found = 0;
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == msg->replica_id) {
            rm.replicas[i].addr = *sender_addr;
            rm.replicas[i].is_alive = 1;
            rm.replicas[i].last_heartbeat = time(NULL);
            replica_found = 1;
            
            // Não loga atualizações de heartbeat
            if (msg->type != HEARTBEAT) {
                log_message(LOG_INFO, "Updated replica %d status: alive=1, port=%d\n",
                          rm.replicas[i].id, ntohs(rm.replicas[i].addr.sin_port));
            }
            break;
        }
    }
    
    if (!replica_found) {
        log_message(LOG_INFO, "Warning: Message from unknown replica %d\n", msg->replica_id);
    }
    
    // Log do estado atual das réplicas apenas para mensagens importantes
    if (msg->type != HEARTBEAT) {
        log_message(LOG_INFO, "Current replica status:\n");
        for (int i = 0; i < rm.replica_count; i++) {
            time_t now = time(NULL);
            log_message(LOG_INFO, "Replica %d: alive=%d, last_heartbeat=%ld seconds ago, port=%d\n",
                      rm.replicas[i].id,
                      rm.replicas[i].is_alive,
                      now - rm.replicas[i].last_heartbeat,
                      ntohs(rm.replicas[i].addr.sin_port));
        }
    }

    switch (msg->type) {
        case START_ELECTION:
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
                    
                    // DEPOIS: Espera um pouco antes de iniciar sua própria eleição
                    // Isso dá tempo para outros servidores responderem
                    usleep(100000);  // 100ms
                    
                    // Só inicia eleição se não estiver em uma
                    if (!rm.election_in_progress) {
                        start_election();
                    } else {
                        log_message(LOG_INFO, "Already in election, not starting new one\n");
                    }
                } else {
                    log_message(LOG_INFO, "Ignoring election from higher ID %d (my_id=%d)\n",
                              msg->replica_id, rm.my_id);
                }
            } else {
                log_message(LOG_INFO, "Ignoring election start, I am primary\n");
            }
            break;
        
        case ELECTION_RESPONSE:
            if (rm.election_in_progress) {
                if (msg->replica_id > rm.my_id) {
                    log_message(LOG_INFO, "Received election response from higher ID %d, stepping down\n",
                              msg->replica_id);
                    rm.election_in_progress = 0;  // Desiste da eleição
                } else {
                    log_message(LOG_INFO, "Ignoring election response from lower ID %d\n",
                              msg->replica_id);
                }
            } else {
                log_message(LOG_INFO, "Received election response but not in election\n");
            }
            break;
            
        case ELECTION_VICTORY:
            // Aceita vitória mesmo se estiver em eleição
            log_message(LOG_INFO, "Received victory from %d (my_id=%d, was_primary=%d, in_election=%d)\n",
                      msg->replica_id, rm.my_id, rm.is_primary, rm.election_in_progress);
            
            // Confirma recebimento da vitória
            replica_message ack;
            memset(&ack, 0, sizeof(ack));
            ack.type = STATE_ACK;  // Usa STATE_ACK como confirmação
            ack.replica_id = rm.my_id;
            ack.timestamp = time(NULL);
            
            // Envia confirmação várias vezes para garantir entrega
            struct sockaddr_in winner_addr;
            memset(&winner_addr, 0, sizeof(winner_addr));
            winner_addr.sin_family = AF_INET;
            winner_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            // Procura porta do vencedor
            for (int i = 0; i < rm.replica_count; i++) {
                if (rm.replicas[i].id == msg->replica_id) {
                    winner_addr.sin_port = rm.replicas[i].addr.sin_port;
                    break;
                }
            }
            
            // Envia confirmação várias vezes
            for (int i = 0; i < 3; i++) {
                ssize_t sent = sendto(replication_socket, &ack, sizeof(ack), 0,
                       (const struct sockaddr*)&winner_addr, sizeof(winner_addr));
                log_message(LOG_INFO, "Sent victory confirmation to winner %d (bytes=%zd)\n", 
                          msg->replica_id, sent);
                usleep(10000); // 10ms entre tentativas
            }
            
            // Atualiza estado
            rm.primary_id = msg->replica_id;
            rm.current_sum = msg->current_sum;
            rm.last_seqn = msg->last_seqn;
            rm.is_primary = 0;  // Garante que não sou primário
            rm.election_in_progress = 0;  // Termina qualquer eleição em andamento
            
            log_message(LOG_INFO, "Updated state: primary=%d, sum=%d, seqn=%lld\n",
                      rm.primary_id, rm.current_sum, rm.last_seqn);
            break;
            
        case JOIN_REQUEST:
            log_message(LOG_INFO, "Received JOIN_REQUEST from replica %d\n", msg->replica_id);
            
            // Adiciona a réplica à lista e marca como viva
            add_replica(msg->replica_id);
            
            if (rm.is_primary) {
                // Se for primário, envia o estado atual
                replica_message state_update;
                memset(&state_update, 0, sizeof(state_update));
                state_update.type = STATE_UPDATE;
                state_update.replica_id = rm.my_id;
                state_update.primary_id = rm.my_id;
                state_update.current_sum = rm.current_sum;
                state_update.last_seqn = rm.last_seqn;
                state_update.timestamp = time(NULL);
                
                // Envia estado várias vezes
                for (int i = 0; i < 3; i++) {
                    sendto(replication_socket, &state_update, sizeof(state_update), 0,
                           (const struct sockaddr*)sender_addr, sizeof(*sender_addr));
                    log_message(LOG_INFO, "Primary sent state update to replica %d (sum=%d, seqn=%lld)\n",
                              msg->replica_id, rm.current_sum, rm.last_seqn);
                    usleep(10000);  // 10ms entre tentativas
                }
            } else {
                // Se for backup, envia apenas confirmação de JOIN
                replica_message join_ack;
                memset(&join_ack, 0, sizeof(join_ack));
                join_ack.type = STATE_ACK;  // Usa STATE_ACK como resposta ao JOIN
                join_ack.replica_id = rm.my_id;
                join_ack.timestamp = time(NULL);
                
                // Envia resposta várias vezes
                for (int i = 0; i < 3; i++) {
                    sendto(replication_socket, &join_ack, sizeof(join_ack), 0,
                           (const struct sockaddr*)sender_addr, sizeof(*sender_addr));
                    log_message(LOG_INFO, "Backup sent JOIN response to replica %d\n", msg->replica_id);
                    usleep(10000);  // 10ms entre tentativas
                }
            }
            break;
            
        case STATE_UPDATE:
            if (!rm.is_primary) {
                // Atualiza estado
                rm.current_sum = msg->current_sum;
                rm.last_seqn = msg->last_seqn;
                rm.primary_id = msg->primary_id;  // Atualiza ID do primário
                rm.received_initial_state = 1;
                
                log_message(LOG_INFO, "Received state update: sum=%d, seqn=%lld\n",
                          msg->current_sum, msg->last_seqn);
                
                // Envia confirmação
                replica_message response;
                memset(&response, 0, sizeof(response));
                response.type = STATE_ACK;
                response.replica_id = rm.my_id;
                response.primary_id = rm.primary_id;
                response.current_sum = rm.current_sum;
                response.last_seqn = rm.last_seqn;
                
                sendto(replication_socket, &response, sizeof(response), 0,
                       (const struct sockaddr*)sender_addr, sizeof(*sender_addr));
            }
            break;
            
        case HEARTBEAT:
            // Se não é primário e recebeu heartbeat do primário, atualiza estado
            if (!rm.is_primary && msg->replica_id == rm.primary_id) {
                rm.current_sum = msg->current_sum;
                rm.last_seqn = msg->last_seqn;
                rm.received_initial_state = 1;
                
                // Log apenas se houver mudança no estado
                static int last_sum = 0;
                if (last_sum != msg->current_sum) {
                    log_message(LOG_INFO, "Received heartbeat from primary: sum=%d, seqn=%lld\n",
                              msg->current_sum, msg->last_seqn);
                    last_sum = msg->current_sum;
                }
            }
            break;
            
        case NEW_PRIMARY:
            if (!rm.is_primary) {
                log_message(LOG_INFO, "Received new primary notification from replica %d\n", msg->replica_id);
                rm.primary_id = msg->replica_id;
                rm.current_sum = msg->current_sum;
                rm.last_seqn = msg->last_seqn;
            }
            break;
            
        case PRIMARY_QUERY:
            if (rm.is_primary) {
                // Responde confirmando que é o primário
                replica_message response;
                memset(&response, 0, sizeof(response));
                response.type = PRIMARY_RESPONSE;
                response.replica_id = rm.my_id;
                response.primary_id = rm.my_id;
                response.current_sum = rm.current_sum;
                response.last_seqn = rm.last_seqn;
                
                sendto(replication_socket, &response, sizeof(response), 0,
                       (const struct sockaddr*)sender_addr, sizeof(*sender_addr));
            }
            break;
            
        default:
            log_message(LOG_WARN, "Unknown message type: %d\n", msg->type);
            break;
    }
    
    pthread_mutex_unlock(&rm.state_mutex);
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
    // Verifica se já existe
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id == replica_id) {
            // Atualiza estado se já existe
            rm.replicas[i].is_alive = 1;
            rm.replicas[i].last_heartbeat = time(NULL);
            return;
        }
    }
    
    // Adiciona nova réplica
    if (rm.replica_count < MAX_REPLICAS) {
        rm.replicas[rm.replica_count].id = replica_id;
        rm.replicas[rm.replica_count].is_alive = 1;  // Começa viva
        rm.replicas[rm.replica_count].last_heartbeat = time(NULL);  // Atualiza timestamp
        
        // Configura endereço da réplica para a porta de replicação (porta + 2)
        rm.replicas[rm.replica_count].addr.sin_family = AF_INET;
        rm.replicas[rm.replica_count].addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        rm.replicas[rm.replica_count].addr.sin_port = htons(replica_id + 2);  // Porta de replicação é porta + 2
        
        log_message(LOG_INFO, "Added new replica %d to list (total=%d), repl_port=%d\n", 
                   replica_id, rm.replica_count + 1, replica_id + 2);
        rm.replica_count++;
    }
}

// Inicializa o gerenciador de replicação
void init_replication_manager(int port, int is_primary) {
    printf("Initializing replication manager on port %d (is_primary=%d)...\n",
           port, is_primary);
    
    // Inicializa estrutura do gerenciador
    memset(&rm, 0, sizeof(rm));
    rm.my_id = port;
    rm.is_primary = is_primary;
    rm.primary_id = is_primary ? port : PRIMARY_PORT;  // Se não é primário, usa porta padrão
    rm.current_sum = 0;
    rm.last_seqn = 0;
    rm.replica_count = 0;
    rm.received_initial_state = is_primary;  // Primário já tem estado inicial
    rm.election_in_progress = 0;  // Inicialmente não está em eleição
    running = 1;
    
    pthread_mutex_init(&rm.state_mutex, NULL);
    
    // Cria socket de replicação
    replication_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (replication_socket < 0) {
        perror("ERROR opening replication socket");
        exit(1);
    }
    
    // Configura endereço do socket de replicação
    struct sockaddr_in repl_addr;
    memset(&repl_addr, 0, sizeof(repl_addr));
    repl_addr.sin_family = AF_INET;
    repl_addr.sin_addr.s_addr = INADDR_ANY;
    repl_addr.sin_port = htons(port + 2);  // Porta de replicação é porta + 2
    
    // Faz bind do socket
    if (bind(replication_socket, (const struct sockaddr*)&repl_addr, sizeof(repl_addr)) < 0) {
        perror("ERROR on binding replication socket");
        exit(1);
    }
    
    printf("Replication service listening on port %d...\n", port + 2);
    
    // Inicia serviços
    pthread_t thread;
    
    // Serviço de listener
    printf("Starting replication listener service...\n");
    pthread_create(&thread, NULL, listener_service, NULL);
    
    // Se não é primário, tenta se juntar ao cluster
    if (!is_primary) {
        // Adiciona primário à lista de réplicas com endereço correto
        rm.replicas[0].id = PRIMARY_PORT;
        rm.replicas[0].is_alive = 1;
        rm.replicas[0].last_heartbeat = time(NULL);
        rm.replicas[0].addr.sin_family = AF_INET;
        rm.replicas[0].addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        rm.replicas[0].addr.sin_port = htons(PRIMARY_PORT + 2);  // Porta de replicação do primário
        rm.replica_count = 1;
        
        log_message(LOG_INFO, "Added primary to replica list (port=%d, repl_port=%d)\n", 
                   PRIMARY_PORT, PRIMARY_PORT + 2);
        
        // Thread para verificar estado do primário
        pthread_create(&thread, NULL, primary_check_service, NULL);
        
        // Envia join request
        if (send_join_request() != 0) {
            printf("Failed to join cluster\n");
            exit(1);
        }
    }
    
    // Serviço de heartbeat (se for primário)
    if (is_primary) {
        printf("Starting heartbeat service...\n");
        pthread_create(&thread, NULL, heartbeat_service, NULL);
    }
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
    log_message(LOG_DEBUG, "Getting current sum: %d\n", sum);
    pthread_mutex_unlock(&rm.state_mutex);
    return sum;
}

// Thread para verificar estado do primário
static void* primary_check_service(void* arg) {
    log_message(LOG_INFO, "Starting primary check service...\n");
    
    while (running) {
        check_primary();
        sleep(CHECK_INTERVAL);
    }
    
    return NULL;
}

// Atualiza o estado do servidor
int update_state(int new_sum, long long seqn) {
    pthread_mutex_lock(&rm.state_mutex);
    
    if (!rm.is_primary) {
        log_message(LOG_WARN, "Warning: Non-primary trying to update state\n");
        pthread_mutex_unlock(&rm.state_mutex);
        return -1;
    }

    rm.current_sum = new_sum;
    rm.last_seqn = seqn;

    // Reseta confirmações
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id != rm.my_id) {
            rm.replicas[i].state_confirmed = 0;
        }
    }

    // Envia atualização para todas as réplicas vivas
    replica_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = STATE_UPDATE;
    msg.replica_id = rm.my_id;
    msg.primary_id = rm.my_id;
    msg.current_sum = new_sum;
    msg.last_seqn = seqn;

    int sent_count = 0;
    for (int i = 0; i < rm.replica_count; i++) {
        if (rm.replicas[i].id != rm.my_id && rm.replicas[i].is_alive) {
            log_message(LOG_INFO, "Sending state update to replica %d: sum=%d, seqn=%lld\n",
                   rm.replicas[i].id, new_sum, seqn);
            
            ssize_t sent = sendto(replication_socket, &msg, sizeof(msg), 0,
                   (const struct sockaddr*)&rm.replicas[i].addr, sizeof(rm.replicas[i].addr));
            
            if (sent > 0) {
                sent_count++;
            }
        }
    }

    // Espera confirmações
    int timeout_ms = REQUEST_TIMEOUT_MS * 2;  // Aumenta o timeout
    int sleep_interval_ms = 100;
    int max_iterations = timeout_ms / sleep_interval_ms;
    int iterations = 0;
    int all_confirmed = 0;

    while (iterations < max_iterations && !all_confirmed) {
        all_confirmed = 1;
        
        // Processa mensagens pendentes
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms
        setsockopt(replication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        replica_message response;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        
        ssize_t n = recvfrom(replication_socket, &response, sizeof(response), 0,
                            (struct sockaddr*)&sender_addr, &addr_len);
                            
        if (n == sizeof(response) && response.type == STATE_ACK) {
            // Procura réplica que enviou a confirmação
            for (int i = 0; i < rm.replica_count; i++) {
                if (rm.replicas[i].id == response.replica_id) {
                    rm.replicas[i].state_confirmed = 1;
                    log_message(LOG_INFO, "Received state confirmation from replica %d\n", 
                              response.replica_id);
                    break;
                }
            }
        }
        
        // Verifica se todas as réplicas vivas confirmaram
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.my_id && rm.replicas[i].is_alive) {
                if (!rm.replicas[i].state_confirmed) {
                    all_confirmed = 0;
                    break;
                }
            }
        }
        
        if (!all_confirmed) {
            iterations++;
            usleep(sleep_interval_ms * 1000);
        }
    }

    if (!all_confirmed) {
        log_message(LOG_WARN, "Warning: Some replicas did not confirm state update within timeout\n");
        
        // Não marca réplicas como mortas imediatamente, apenas avisa
        for (int i = 0; i < rm.replica_count; i++) {
            if (rm.replicas[i].id != rm.my_id && rm.replicas[i].is_alive && !rm.replicas[i].state_confirmed) {
                log_message(LOG_WARN, "Warning: No confirmation from replica %d\n", rm.replicas[i].id);
            }
        }
        
        pthread_mutex_unlock(&rm.state_mutex);
        return -1;
    }

    pthread_mutex_unlock(&rm.state_mutex);
    return 0;  // Sucesso
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
        
        // Tenta portas de 2000 a 2010 (ajuste conforme necessário)
        for (int port = 2000; port <= 2010; port += 4) {
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
