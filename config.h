#ifndef CONFIG_H
#define CONFIG_H

// Configurações de rede
#define MAX_CLIENTS 10      // Máximo de clientes simultâneos
#define BUFFER_SIZE 1024    // Tamanho do buffer de rede

// Configurações de replicação
#define REPLICA_TIMEOUT 3         // Reduzido de 5s para 3s
#define HEARTBEAT_INTERVAL_MS 1000  // Intervalo de heartbeat em ms
#define ELECTION_TIMEOUT_MS 5000    // Timeout para eleição em ms

// Portas base
#define BASE_PORT 2000      // Porta base para servidores
#define PRIMARY_PORT 2000   // Porta base do servidor primário
#define PORT_STEP 4         // Incremento de porta entre servidores

// Timeouts e delays
#define SOCKET_TIMEOUT_MS 500    // Timeout para operações de socket
#define DISCOVERY_RETRY_MS 100   // Reduzido de 500ms para 100ms
#define DISCOVERY_TIMEOUT_MS 1000 // Reduzido de 2000ms para 1000ms
#define REQUEST_TIMEOUT_MS 500    // Reduzido de 2000ms para 500ms
#define PRIMARY_TIMEOUT 5         // Reduzido de 10s para 5s para detectar falha mais rápido
#define JOIN_TIMEOUT 5            // Reduzido de 10s para 5s
#define CHECK_INTERVAL 1          // Mantido em 1s

// Limites e capacidades
#define MAX_RETRIES 3        // Número máximo de tentativas
#define MAX_SERVERS 10       // Número máximo de servidores
#define MAX_REPLICAS 10      // Número máximo de réplicas
#define MAX_MESSAGE_LEN 1024 // Tamanho máximo de mensagem

#endif
