/*=====================================================================================================================
	Este arquivo possui apenas um esqueleto de como deve funcionar o server de controle, algumas partes dele
devem ser recicladas (provavelmente 100%) doq já possuímos nos servers participantes (os que farão as somas no trabalho),
principalmente as inicializações e conexões com os clientes, pois do ponto de vista do controle, os servers participantes
são clientes
=======================================================================================================================*/

void new_server_connection();// Pegar de base a conexão de um novo cliente, deve funcionar tranquilamente, porem devemos acrescentar a propagação da soma e da lista de servers ja cadastrados

//O primeiro servidor cadastrado deve ser o lider
void first_server(int* server.leader){
	server.leader  = 1;
}

/*Essa funcao faz uma checagem se o novo server tem id maior, para invocar a eleicao
*/

int is_new_server_bigger(SERVER_DATA* server_list, SERVER_DATA new_server){
	while((new_server.id <= server_list.id) || (server_list == NULL)){
		server_list = server_list.ntx;
	}
	if (server_list != NULL) begin_election;
}


/*Essa funcao faz uma checagem periodica de SLEEP_TIMEOUT segundos para ver se o lider esta online
*/
int is_leader_alive(int leader_id, int socket_leader){
	int alive = 1;
	char buffer_receive[50]; // Buffer para a resposta de alive caso venha fora de sincronia
	
	while(alive == 1){
		if (strcmp(receive(buffer_receive,socket_leader),NULL)) alive = 0;
		//A checagem eh se houve alguma resposta do servidor lider que seja o TOKEN_IS_ON, portanto o lider estaria online, se nao houve, o buffer estara vazio
		//Como o servidor controle sempre estara escutando o servidor lider no socket, as unicas mensagens que devem chegar do LIDER para o controle eh se esta online ou nao
		//O buffer_receive é o destino e o socket_leader é a fonte
		sleep(SLEEP_TIMEOUT);
	}
	return alive;
}
	

/* Essa funcao fara o controle iniciar uma eleicao, ele aguarda por SLEEP_TIMEOUT segundos do servidor lider, caso nao haja resposta, inicia uma eleicao
o server lider esta identificado como leader_id,
*/
int begin_election(int leader_id, int socket_leader, SERVER_DATA* server_list){
	ELECTION_DATA* candidatos, aux, inicio_cand = aux;
	SERVER_DATA* inicio_lista = server_list;
	char buffer_candidatos[MAX_SERVERS]
	int new_leader;
	int i = 0;
	
	aux.server_id = 0
	sux.joined = 0;
	aux.nxt = candidatos;
	while(1){
		if(!is_leader_alive(leader_id,socket_leader)){
			while (server_list.nxt != NULL){
				send_message(ELECTION_TOKEN,server_list.socket); // Considerando uma lista encadeada, o ultimo termo eh o q possui server_nxt == NULL, se trocar por um array adaptar o laco
				// Susbtituir pela funcao implementada, onde ELECTION_TOKEN eh a mensagem a ser enviada e socket o destino
				server_list = server_list.nxt;
}		
		server_list = inicio_lista;
		sleep(JOIN_TIME);
		while (server_list.nxt != NULL){
			receive(candidatos, server_list.socket); // Substituir pela funcao receive implementada com os argumentos adequados, aux eh o destino e o socket a fonte
			server_list = server_list.nxt;
			i++;
			aux.nxt = candidatos;
			aux = candidatos;
		}
		
		server_list = inicio_lista;
		new_leader = bully_election(inicio_cand);
		return new_leader;
		}
	}
}
	

/* Funcao de eleicao, que recebe uma lista encadeada dos servidores participantes (pode alterar por um array mas teria que alterar os laços de controle)
Para sinalizar aos servidores que uma eleicao foi iniciada eh enviado um token "#E#L#E#E#C#T#I#O#N", para isso apenas foi chamada uma funcao send_message que deve
ser trocada pela funcao que implementamos, com os parametros adequados
*/
int bully_election(ELECTION_DATA* server_list){
	int leader_id = server_list[0].server_id;
	int i;
	int socket_new_leader;
	ELECTION_DATA* inicio_lista;
	
	while(server_list != NULL){
		if (server_list[i].server_id > leader_id) leader_id = server_list[i].server_id;
		server_list = server_list.nxt;
	}
	server_list = inicio_lista;
	while(server_list != NULL){
		if (server_list.server_id == leader_id) send_message(ELECTION_RESULT,server_list.socket); // Substituir pela send implementada, ELECTION_RESULT é a mensagem, socket o destino
		server_list = server_list.nxt;
	}
	return leader_id;
}
	
	
	
	
	
	
	
	
	
	
	
	
	
	
//main()

void control_server(){
	//Copiar toda a parte de inicializacao, sockets e conexão doq já possuimos dos servers porem com algumas ressalvas que explicitarei aqui
	
SERVER_DATA leader;
SERVER_DATA* server_list;
int first_conn = 1;
	
	while(this_server_is_on){
		new_server_connection(new_server);
		if (first_conn){ //Se for o 1º server a conectar ele é o lider
			leader = new_server;
			first_conn = 0;
		}
		fork(); // Assim como nos servers, um fork para escutar
		is_new_server_bigger(new_server,server_list);
		is_leader_alive(leader.id,leader.socket);
		if(!is_leader_alive) leader = begin_election(leader.id,leader.socket,server_list);
	}
}
