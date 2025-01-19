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

// Tipos de mensagens de replicação
typedef enum {
    JOIN_REQUEST,     // Pedido para juntar-se ao cluster
    STATE_UPDATE,     // Atualização de estado
    STATE_ACK,       // Confirmação de atualização de estado
    HEARTBEAT,       // Heartbeat do primário
    NEW_PRIMARY,     // Notificação de novo primário
    PRIMARY_QUERY,   // Consulta se um servidor é primário
    PRIMARY_RESPONSE, // Resposta confirmando que é primário
    START_ELECTION,  // Inicia processo de eleição
    ELECTION_VOTE,   // Voto em uma eleição
    ELECTION_RESPONSE, // Resposta a uma eleição (usado no Bully Algorithm)
    ELECTION_VICTORY // Anúncio do vencedor da eleição
} message_type;

// Estrutura para mensagens de replicação
typedef struct {
    message_type type;          // Tipo da mensagem
    int primary_id;            // ID do primário
    int replica_id;           // ID da réplica
    int current_sum;         // Soma atual
    long long last_seqn;    // Último número de sequência
    time_t timestamp;       // Timestamp da mensagem
    int state_confirmed;    // Flag para confirmação de estado
} replica_message;

// Estrutura para réplicas
typedef struct {
    int id;                     // ID da réplica (porta)
    struct sockaddr_in addr;    // Endereço da réplica
    int is_alive;              // Se a réplica está viva
    time_t last_heartbeat;     // Timestamp do último heartbeat
    int state_confirmed;       // Se confirmou recebimento do último estado
} replica;

// Estrutura do gerenciador de replicação
typedef struct {
    int my_id;                  // ID deste servidor
    int primary_id;             // ID do servidor primário
    int is_primary;             // Se este servidor é o primário
    int current_sum;            // Soma atual
    long long last_seqn;        // Último número de sequência
    int received_initial_state; // Se recebemos o estado inicial
    time_t last_primary_check;  // Último check do primário
    time_t join_time;          // Momento em que a réplica se juntou
    pthread_mutex_t state_mutex;// Mutex para o estado
    replica replicas[MAX_REPLICAS];  // Lista de réplicas
    int replica_count;          // Número de réplicas
    int election_in_progress;    // Flag para indicar eleição em andamento
} replication_manager;

// Constantes
#define MAX_REPLICAS 10
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

#endif // REPLICATION_H
