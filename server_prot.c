/*==============================================================================================================================
	Se pesquisar "IMPLEMENTAR" neste arquivo ira destacar aonde tem as funcoes ainda a serem implementadas
	Se pesquisar "CONFERIR" neste arquivo ira destacar aonde estao as duvidas de implementacao
	
	
	Este programa junta 2 programas disponiveis no moodle, o "Algoritmo de Lamport - Algoritmo da Padaria" 
	e tambem o "Exemplo 3 - POSIX variaveis de condicao" para atender as especificações do trabalho.
	
	Do "Algoritmo de Lamport" foi utilizado principalmente a parte de criacao e join de threads, ignorando a parte do
	"Algoritmo da padaria".
	Do "Exemplo 3" foi utilizado principalmente a estrutura do "Produtor Consumidor", acrescentando uma nova secao 
	critica no produtor para nao haver inconsistencias na execucao (ver o codigo abaixo).
	
	As partes comentadas entre "--" sao trechos do codigo original do moodle.
	
	O arquivo "server_prot.h" possui o cabecalho das funcoes a serem implementadas e tambem as structs utilizadas, 
	podendo realizar alteracoes se necessario ou conforme for a implementacao desejada.
	
	Eu disponibilizei as structs que eu utilizei nesta implementacao as structs como comentarios para facilitar 
	a compreensao.
	
	No meio do codigo possuem comentarios para ajudar a guiar a compreensao de trechos especificos, e daonde 
	eu fiz alteracoes no codigo original
===================================================================================================================================*/
	





#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "server_prot.h"

/* Variaveis compartilhadas */

pthread_mutex_t mutex, mutex_op, mutex_op2;
int server_acc = 0;
int server_running = 0;
int thread_num = 0;
int global_simple_id = 0;
int count = 0, in = 0, out = 0;
int buffer[MAX_BUFFER];
char porta[MAX_BUFFER];

/*================================================================================
	Estruturas usadas no servidor
==================================================================================
typedef struct request_data{
	int		request_value;
	int		request_id;
	int		processed;
}	REQUEST_INFO;

typedef struct client_data{
	char	client_id[ID_BUFFER];
	REQUEST_INFO	request[MAX_BUFFER];
	
//Aqui foi apenas uma ideia para nao ter de trabalhar com o endereco MAC podemos
//fazer uma funcao que gere um id simplificado, mas se quiserem trabalhar com o 
//id completo tem de adaptar o codigo para trabalhar com strings

	int		client_simple_id;
	int		client_received_value[MAX_BUFFER];
	int		is_connected;
	int		last_value;
}	CLIENT_INFO;

=====================================================================================*/




 void *produtor(void *arg) {
	 
	 
//Casting CONFERIR
	 CLIENT_INFO* temp_client = *((CLIENT_INFO*)arg);

     while (TRUE) {
		 //-- sleep(rand()%5); --
	     pthread_mutex_lock(&mutex);
	     while (count == MAX_BUFFER)
	         pthread_cond_wait(&cond_empty, &mutex);


// Alteracao 1 - Trocar a insercao por uma funcao A IMPLEMENTAR AINDA!!
	     client_input_value(&buffer[in]); // IMPLEMENTAR a funcao que fica aguardando (listening) o cliente enviar um valor
		 
		 
	     count++;
         printf("Client inseriu no buffer[%d] = %d\n", in, buffer[in]);
		 
/* 
	Alteracao 2 - Lockar um mutex para a soma no acumulador do servidor
	A funcao pthread_mutex_trylock(&mutex_op2) tenta lockar um mutex, retorna 0 se deu tudo certo, -1 se algum erro
*/
		while(!pthread_mutex_trylock(&mutex_op2)) wait_for_unlock(mutex_op2); //IMPLEMENTAR a funcao wait_for_unlock apenas para ficar ocioso (idle) ate liberar o mutex, pode ser apenas um loop com return
		
// Soma no acumulador e retorna ja no cliente o ultimo valor que ele recebeu, esta parte de retorno foi cortada do consumidor pois pode haver alteracoes na execucao	
		 server_acc = server_acc + buffer[in];
		 (*temp_client).last_value = server_acc;
		 pthread_mutex_unlock(&mutex_op2);
		 
         in = (in + 1) % MAX_BUFFER;
	     pthread_cond_signal(&cond_full);
	     pthread_mutex_unlock(&mutex);
     }
 }

 void *consumidor(void *arg) {
     while (TRUE) {
		 //-- sleep(rand()%5); --
	     pthread_mutex_lock(&mutex);
	     while (count == 0)
	         pthread_cond_wait(&cond_full, &mutex);
	     my_task = buffer[out];
	     count--;
         printf("Processando o valor do client buffer[%d] = %d\n", out, my_task);
         out = (out + 1) % MAX_BUFFER;
	     pthread_cond_signal(&cond_empty);
	     pthread_mutex_unlock(&mutex);
     }
 }
 
 
 
 
 
/*===================================================================================
	1 thread = 1 client conectado
	Recebe no *arg um CLIENT_INFO 
	Faz o casting CONFERIR!!!
	Cria um fork para ficar "listening"
	Encerra quando o cliente desconecta
=====================================================================================*/
static void * client_thread(void *arg){
	CLIENT_INFO* this_client;
	pid_t p1;
	int request_number = 0;
	pthread_t prod, cons;
	
//Fazer o casting corretamente CONFERIR!!!!!
	this_client = *((CLIENT_INFO*)arg);
	
	
// Fazer um fork para um processo filho aguardar que o cliente disconecte
	p1 = fork();
	if(p1 == 0){
		wait_disconnect(this_client); // IMPLEMENTAR funcao que vai alterar a variavel this_client.is_connected para 0 ou false se quiser mudar o tipo da variavel
	}

// Algoritmo do produtor consumidor do moodle - adptado para mais um lock pra somar no acumulador - enquanto o cliente estiver conectado
	while((*this_client).is_connected != 0){
		pthread_cond_init(&cond_empty, NULL);
		pthread_cond_init(&cond_empty, NULL);
		pthread_cond_init(&cond_full, NULL);
		pthread_mutex_init(&mutex, NULL);
		pthread_mutex_init(&mutex_op2, NULL);

		pthread_create(&prod, NULL, (void *)produtor, NULL);
		pthread_create(&cons, NULL, (void *)consumidor, NULL);
		
	}
	pthread_exit(0);

}
	
	
	







/*========================================================================================
	Funcionamento do servidor:
	1- Inicializacao  de variaveis
	2 - Fork para criar um processo que aguarda o encerramento do servidor (idle)
	3 - Fork para criar um processo que aguarda conexoes com clientes (idle)
			->Ao atingir a conexao ele pega os dados do cliente e incrementa o indice do buffer
	4 - Criacao da thread para o cliente
	
	
==========================================================================================*/
void server_functioning(int argc, char **argv)
{
    int ret, ret2[MAX_CLIENTS], cont = 0;
    pthread_t tinfo_process[MAX_CLIENTS];
    int tinfo_id[N];
	int last_client;
	CLIENT_INFO new_client[MAX_CLIENTS];
    pthread_attr_t attr;
    void *res;
	pid_t p,p2;
    char porta[MAX_BUFFER];
	

	
// Inicializacao de variaveis - CONFERIR SE TA PEGANDO A PORTA DE MANEIRA CERTA
	strtok(*argv," ");
	porta = strtok(NULL," ");
	
	printf("A porta de acesso eh %s \n",porta);
	server_running = 1;
    ret = pthread_attr_init(&attr);
	for(cont = 0; cont < MAX_CLIENTS; cont++) ret2[cont] = pthread_attr_init(&attr);
	
// Criacao de um processo filho para aguardar o encerramento do servidor
	p = fork();
	if(p == 0){
		while(server_running != 0)
		server_running = wait_closure(); // IMPLEMENTAR a funcao que vai mudar o server_running para 0 ou false se quiser mudar o tipo da variavel
	}
    // -- for (thread_num = 0; thread_num < N; thread_num++)-- 
		
//Enquanto o servidor estiver online
	while(server_running != 0){
		// --tinfo_id [thread_num] = thread_num;--
		
//Criacao de um fork para ficar aguardando conexoes com os clients, criando um novo client
		p2 = fork();
			
		if(p2 == 0){
			while(server_running != 0){
			wait_connection(); // IMPLEMENTAR a funcao que vai estabelecer a conexao entre um cliente e o servidor
			get_client_data(new_client[thread_num]); // IMPLEMENTAR funcao que "cadastra" um cliente no vetor de new_clients
			thread_num++;
			}
		}			
				
//Criacao de uma thread por cliente, a funcao client thread vai tratar das solicitacoes de cada cliente
				ret2[thread_num] = pthread_create(&tinfo_process[thread_num], &attr, &client_thread, new_client[thread_num]);
			}

// Encerramento do server e join das threads
    ret = pthread_attr_destroy(&attr);
    
	last_client = thread_num;
    for (thread_num = 0; thread_num < last_client; thread_num++) {
    	ret = pthread_join(tinfo_process[thread_num], &res);
    	printf("Joined with thread id %d\n", thread_num);
    	free(res);
    }

    exit(EXIT_SUCCESS);
}