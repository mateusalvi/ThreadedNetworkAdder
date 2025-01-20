#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"

// Tipos de mensagem
typedef enum {
    HEARTBEAT = 1,
    JOIN_REQUEST,
    STATE_UPDATE,
    STATE_ACK,
    REPLICA_LIST_UPDATE,
    START_ELECTION,
    ELECTION_RESPONSE,
    VICTORY,
    VICTORY_ACK
} message_type;

// Estrutura para informações de uma réplica
typedef struct {
    int id;
    int is_alive;
    time_t last_heartbeat;
    struct sockaddr_in addr;
    int state_confirmed;
} replica_info;

// Estrutura de mensagem de replicação
typedef struct {
    message_type type;
    int replica_id;
    int primary_id;
    int current_sum;
    long long last_seqn;
    time_t timestamp;
    int replica_count;
    replica_info replicas[10];
} replica_message;

// Estrutura do gerenciador de replicação
typedef struct {
    int my_id;
    int is_primary;
    int primary_id;
    int current_sum;
    long long last_seqn;
    int received_initial_state;
    int election_in_progress;
    replica_info replicas[10];
    int replica_count;
    pthread_mutex_t state_mutex;
} replication_manager;

// Constantes
#define MAX_REPLICAS 10
#define PRIMARY_TIMEOUT 5
#define HEARTBEAT_INTERVAL 1  // Intervalo de heartbeat em segundos
#define CHECK_INTERVAL 1      // Intervalo de verificação do primário em segundos
#define STATE_TIMEOUT 2       // Timeout para confirmação de estado em segundos
#define PRIMARY_PORT 2000     // Porta do servidor primário inicial

// Funções exportadas
void init_replication_manager(int port, int is_primary);
void stop_replication_manager(void);
// Atualiza o estado do servidor
// Retorna 0 em caso de sucesso, -1 em caso de erro
int update_state(int new_sum, long long seqn);
int is_primary(void);
int get_current_sum(void);
void add_discovered_replica(const char* ip, int port);  // Nova função para adicionar réplica descoberta

#endif // REPLICATION_H
