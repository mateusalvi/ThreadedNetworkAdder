
/*	Devemos alterar o cdogio original para incluir a struct SERVER_DATA, e inicializar adequadamente com o 1º server sendo o lider
e a cada server inicializado atualizar tanto qual é o lider e cadastrar um novo server na server_list.

	PRECISA IMPLEMENTAR UMA FUNÇÃO PARA DETECTAR UM TIME_OUT ENTRE OS SERVERS !!!!!! 
	
	Uma ideia seria fazer um fork que fica checando o status dos outros servidores na inicialização do servidor parecido com o que foi 
feito na etapa 1 para escutar os clientes, porem para isso é necessário haver um jeito de diferenciar a conexão de servidores entre si
da conexão com um cliente. Podemos assumir que vão existir duas etapas de conexão, uma apenas com servidores e outra que abrirá o canal
para clientes conectarem, mas isso é apenas uma ideia, se tiver uma ideia mais prática podem ignorar.

	Para evitar que o algoritmo de bully seja rodado a cada time out, já possuímos a lista de servidores conectados ordenados crescentemente
pelo id e quando o líder desconectar apenas buscamos o próximo server_id mais alto na server_list.
	E caso um servidor novo for conectado com um id maior, ele é cadastrado na server_list e passa a ser o novo líder.
	Para distinguir um servidor líder dos outros podemos apenas acrescentar um (if this_server.leader != 0 then operate).
	O server id pode ser inicializado manualmente com um scanf via teclado na máquina ou com um número aleatório porem caso optemos
pelo número aleatório podem existir duplicatas e devem ser tratadas, é mais simples atribuir manualmente para não haver esse tratamento
*/




#define MAX_SERVERS 10

/* Necessario nao declara o vetor diretamente pois para descobrir o final da lista se checa (if nxt_server == NULL); */
typedef struct server_info{
	int	server_id;
	int	leader = 0; // Inicialização com 0 (false)
	struct server_info* nxt_server = NULL, prv_server = NULL; // Inicialização com NULL
} SERVER_DATA

//Incluir na main do servidor as declarações de ponteiros e a criação das variáveis SERVER_DATA
SERVER_DATA* server_list;
SERVER_DATA* new_server;
server_list = malloc(sizeof(SERVER_DATA));
new_server = malloc(sizeof(SERVER_DATA));



// Implementação de inicialização de servidor via teclado
void server_initializer( SERVER_DATA* new_server){
	int new_id;
	fflush(stdin);
	printf("Please enter the server id: ");
	scanf("%d",&new_id);
	new_server.server_id = new_id;
}

/* Implementação de inclusao de novo servidor em ordem CRESCENTE de id, e caso new_server.id seja maior ja torna o lider,

Entradas: ponteiro para a lista, ponteiro para o novo servidor, ponteiro do ponteiro para o começo a lista

saída: Lista reordenada com new_server posicionado, e se o new_server.id for o maior ele já sai como líder e como novo começo
da server_list
*/
void server_inclusion(SERVER_DATA* server_list, SERVER_DATA* new_server, SERVER_DATA** server_list_begin){
	int i = 0;
	SERVER_DATA* temp = server_list;

	while((*server_list).nxt_server != NULL){ // Checagem se a lista esta vazia
		i++;
		server_list = (*server_list).nxt_server;
	}
	if (i = 0){ // Se i = 0 significa q não havia servidores na lista
		server_list_begin = new_server;
	}
	
	else{
	i = 0;
	server_list = temp;
	
	while((*server_list).server_id > (*new_server).server_id){ // Encontrar o 1º server_id menor que o new_server.id
		i++;
		server_list = (*server_list).nxt_server; // Ao fim server_list estará contendo o endereço do 1º server com id menor
	}
	
	
	(*new_server).nxt_server = server_list; //Ajuste de chaveamento para incluir new_server na posicao certa
	(*new_server).prv_server = (*server_list).prv_server;
	(*server_list).prv_server = new_server;
	
	if(i = 0){ // Se i = 0 significa q o new_server.id é o maior e deve ser o novo líder e a lista deve ter um novo inicio
		(*new_server).leader = 1;
		(*server_list).leader = 0;
		server_list_begin = new_server;
	}	
}
}


/* Método a ser chamado caso haja um time_out
Entradas: ponteiro para server_list, e ponteiro do ponteiro onde começa a server_list
saída: nova server_list sem o 1º elemento antes da chamada do método, e o novo começo da lista apontando para o próximo
servidor da lista
*/
void new_leader(SERVER_DATA* server_list, SERVER_DATA** server_list_begin){ 
	SERVER_DATA* temp = (*server_list).nxt_server;
	
	(*temp).prv_server = NULL;
	(*temp).leader = 1;
	server_list_begin = temp;

}
	
