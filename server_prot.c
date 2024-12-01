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

#include "server_prot.h"

pthread_mutex_t sumRequestMutex, mutex_op, mutex_op2;
pthread_cond_t sumRequestQueueEmpty, sumRequestQueueFull, newClientsEmpty, newClientsFull;
bool isRunning = false;
int server_acc = 0;
int thread_num = 0;
int global_simple_id = 0;
int count = 0, in = 0, out = 0;
int buffer[MAX_BUFFER];
char newClientPort;

void *SumRequest_Producer(void *arg)
{
	// Casting CONFERIR
	CLIENT_INFO *this_client = ((CLIENT_INFO *)arg);
	static char returnMessage[MAX_MESSAGE_LEN];
	while ((*this_client).is_connected != 0)
	{
		//-- sleep(rand()%5); --
		pthread_mutex_lock(&sumRequestMutex);
		while (count == MAX_BUFFER)
			pthread_cond_wait(&sumRequestQueueEmpty, &sumRequestMutex);
		// Alteracao 1 - Trocar a insercao por uma funcao A IMPLEMENTAR AINDA!!
		// client_input_value(&buffer[in]); // IMPLEMENTAR a funcao que fica aguardando (listening) o cliente enviar um valor

		int request = ListenForAddRequest((*this_client).port, (*this_client).IP);

		adder_implementation(request, 10, &buffer[in], returnMessage);
		

		count++;
		printf("Client inseriu no buffer[%d] = %d\n", in, buffer[in]);

		/*
			Alteracao 2 - Lockar um mutex para a soma no acumulador do servidor
			A funcao pthread_mutex_trylock(&mutex_op2) tenta lockar um mutex, retorna 0 se deu tudo certo, -1 se algum erro
		*/
		while (pthread_mutex_trylock(&mutex_op2) != 0)
		{
			// DO NOTHING
			// wait_for_unlock(mutex_op2); // IMPLEMENTAR a funcao wait_for_unlock apenas para ficar ocioso (idle) ate liberar o mutex, pode ser apenas um loop com return (M: pra que?)
		}

		// Soma no acumulador e retorna ja no cliente o ultimo valor que ele recebeu, esta parte de retorno foi cortada do consumidor pois pode haver alteracoes na execucao
		server_acc = server_acc + buffer[in];
		(*this_client).last_value = server_acc;
		pthread_mutex_unlock(&mutex_op2);
		in = (in + 1) % MAX_BUFFER;
		pthread_cond_signal(&sumRequestQueueFull);
		pthread_mutex_unlock(&sumRequestMutex);

		// SendMessage("I DID YOUR SUM!", (*this_client).IP, (*this_client).port, returnMessage, false);
	}
}

void *AckSumRequest_Consumer(void *arg)
{
	// Conferir casting
	CLIENT_INFO *this_client = ((CLIENT_INFO *)arg);

	while ((*this_client).is_connected != 0)
	{
		static char returnMessage[MAX_MESSAGE_LEN] = "";
		//-- sleep(rand()%5); --
		pthread_mutex_lock(&sumRequestMutex);
		while (count == 0)
			pthread_cond_wait(&sumRequestQueueFull, &sumRequestMutex);
		int my_task = buffer[out];
		count--;
		printf("Processando o valor do client buffer[%d] = %d\n", out, my_task);

		SendMessage("I DID YOUR SUM!", (*this_client).IP, (*this_client).port, returnMessage, false); // TODO IMPLEMENTAR a funcao que retorna pro cliente //M: Já existe -> SendMessage(ip, porta, message)
		out = (out + 1) % MAX_BUFFER;
		pthread_cond_signal(&sumRequestQueueEmpty);
		pthread_mutex_unlock(&sumRequestMutex);
	}
}

/*
1 thread = 1 client conectado
Recebe no *arg um CLIENT_INFO
Faz o casting <CONFERIR!!!>
Cria um fork para ficar "listening"
Encerra quando o cliente desconecta
*/
static void *ClientHandlerThread(void *arg)
{
	CLIENT_INFO *this_client;
	pid_t p1;
	int request_number = 0;
	pthread_t prod, cons;
	void *null;
	// Fazer o casting corretamente CONFERIR!!!!!
	this_client = ((CLIENT_INFO *)arg);
	// Fazer um fork para um processo filho aguardar que o cliente disconecte
	// p1 = fork();
	// if (p1 == 0)
	// {
	// 	wait_disconnect(this_client); // IMPLEMENTAR funcao que vai alterar a variavel this_client.is_connected para 0 ou false se quiser mudar o tipo da variavel
	// }
	// else
	// {
	// Algoritmo do produtor consumidor do moodle - adptado para mais um lock pra somar no acumulador - enquanto o cliente estiver conectado

	while ((*this_client).is_connected != 0)
	{
		pthread_cond_init(&sumRequestQueueEmpty, NULL);
		pthread_cond_init(&sumRequestQueueEmpty, NULL);
		pthread_cond_init(&sumRequestQueueFull, NULL);
		pthread_mutex_init(&sumRequestMutex, NULL);
		pthread_mutex_init(&mutex_op2, NULL);

		pthread_create(&prod, NULL, (void *)SumRequest_Producer, &(*this_client));
		pthread_create(&cons, NULL, (void *)AckSumRequest_Consumer, &(*this_client));

		pthread_join(prod, null);
		pthread_join(cons, null);
	}
	pthread_exit(0);
	// }
}

/*
Funcionamento do servidor:
1 - Inicializacao  de variaveis
2 - Fork para criar um processo que aguarda o encerramento do servidor (idle)
3 - Fork para criar um processo que aguarda conexoes com clientes (idle)
	->Ao atingir a conexao ele pega os dados do cliente e incrementa o indice do buffer
4 - Criacao da thread para o cliente
*/

void ServerMain(char *port)
{
	int ret, ret2[MAX_CLIENTS];
	pthread_t tinfo_process[MAX_CLIENTS];
	int tinfo_id[MAX_CLIENTS];
	int last_client;

	pthread_attr_t attr;
	void *res;
	pid_t p, p2;

	// Inicializacao de variaveis - CONFERIR SE TA PEGANDO A PORTA DE MANEIRA CERTA
	// strtok(*argv, " "); // Mateus: ??
	// int porta = strtok(NULL, " ");
	// printf(porta);

	printf("A porta de acesso eh %s \n", port);
	isRunning = true;
	ret = pthread_attr_init(&attr);

	for (int i = 0; i < MAX_CLIENTS; i++)
		ret2[i] = pthread_attr_init(&attr);

	// Criacao de um processo filho para aguardar o encerramento do servidor
	p = fork();
	if (p == 0)
	{
		while (isRunning != false)
			isRunning = wait_closure(); // IMPLEMENTAR a funcao que vai mudar o server_running para 0 ou false se quiser mudar o tipo da variavel
	}
	else
	{
		// Criacao de um fork para ficar aguardando conexoes com os clients, criando um novo client
		p2 = fork();
		if (p2 == 0)
		{
			while (isRunning != 0)
			{
				CLIENT_INFO *newClient = ListenForNewClients(atoi(port)); // Start the P2 subprocess
				if (newClient != NULL)
				{
					thread_num++;
					printf("Thread num %d \n", thread_num);
					ret2[thread_num] = pthread_create(&tinfo_process[thread_num], &attr, &ClientHandlerThread, &(*newClient));
				}
				// Criacao de uma thread por cliente, a funcao client thread vai tratar das solicitacoes de cada cliente
				// }
				// Encerramento do server e join das threads
			}
		}
		else // Parent
		{
			// ret2[thread_num] = pthread_create(&tinfo_process[thread_num], &attr, &ClientHandlerThread, &clients[thread_num]);
			// // Criacao de uma thread por cliente, a funcao client thread vai tratar das solicitacoes de cada cliente
			// // }
			// // Encerramento do server e join das threads

			ret = pthread_attr_destroy(&attr);

			last_client = thread_num;
			for (thread_num = 0; thread_num < last_client; thread_num++)
			{
				ret = pthread_join(tinfo_process[thread_num], &res);
				printf("Joined with thread id %d\n", thread_num);
				free(res);
			}

			while (isRunning)
			{
				/* KEEP THE PARENT PROCESS ALIVE */
			}
		}
	}

	printf("You should not be reading this message.");
	exit(EXIT_SUCCESS);
}

int wait_closure()
{
	printf("Entered %s\n", __func__);
	while (true)
	{
	}
	return 0;
}

void return_value_to_client(int value)
{
	printf("Entered %s\n", __func__);
	return;
}

void client_input_value(int *buffer)
{
	printf("Entered %s\n", __func__);
	return;
}

void wait_for_unlock(pthread_mutex_t mutex_A)
{
	// printf("Entered %s\n", __func__);

	return;
}

void wait_disconnect(CLIENT_INFO *this_client)
{
	printf("Entered %s\n", __func__);
	return;
}