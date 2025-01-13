#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SLEEP_TIMEOUT 5
#define ELECTION_TOKEN "#E#L#E#E#C#T#I#O#N"
#define TOKEN_IS_ON "#I#A#M#O#N#"
#define ELECTED_TOKEN "#E#L#E#C#T#E#D#"
#define JOIN_TIME 2
#define ELECTION_RESULT "#W#I#N#N#E#R"

typedef struct election_form{
	int server_id;
	struct election_form* nxt = NULL;
} ELECTION_DATA;

/*===========================================================================================================================================================
	As implementacoes tomam em conta que as funcoes de send e receive foram implementadas, tambem considere que essas funções rodam nos servidores participantes
e fazem as trocas de dados com um servidor separado que faz o controle dos outros servidores, e nesse contexto o servidor de controle seria o "server" e os 
servidores participantes do trabalho seriam "clientes". O servidor de controle deve ser inicializado e deve possuir uma conexao (socket) com cada servidor
participante do trabalho.

	Em resumo é necessario criar um servidor de controle que vai gerenciar os servidores ativos, e as funcoes descritas aqui devem ser chamadas
pelos servidores participantes do trabalho e não no servidor de controle. As funcoes a serem executadas no servidor de controle estarao mais abaixo e bem destacadas.
Esse servidor de controle terá a mesma estrutura dos servidores participantes no que diz respeito as trocas de mensagens (sockets), portanto RECICLAR AO MÁXIMO
AS FUNÇÕES DE CADASTRAR NOVO CLIENTE, ESTABELECER CONEXÃO, SEND, RECEIVE, ETC... só que adaptadas para o contexto de servidores (imagino que não devamos mudar nada 
noq tange o código pois da visão do controle o server participante se comporta igual como um cliente)

	As trocas de mensagens serão como comandos onde o receptor toma uma ação a partir do comando recebido
	
============================================================================================================================================================*/	

/* Essa funcao apenas informa aos outros servidores que este servidor identificado por this_server está online, e com uma flag/token indicando que esta online
Para uso futuro eh necessario q este token tenha um tipo definido, que para simplificaçao sera um string no define.
Caso seja necessário alterar o tipo do this_server_id por semantica nao teremos problemas
   Para que a funcione adequadamente a funcao q envia uma mensagem broadcast deve funcionar, ou uma funcao que envie especificamente para os servidores
participantes e não para os clientes
*/

void is_alive(int this_server_id, int is_online, int socket_control_server){
	
	while(is_online == 1){ //Pode trocar o tipo de is_online para bool e substituir por while( is_online == TRUE)
		printf("Eu sou o servidor %d e estou funcionando \n",this_server);
		send_message(TOKEN_IS_ON, socket_control_server); // Substituir pela funcao implementada, com os argumentos adequados, o token é a mensagem e o socket o destino
		//Relembrando que o token pode ser de qualquer tipo, e a string "Estou funcionando" eh apenas para visualização no terminal que o server está online
		sleep(SLEEP_TIMEOUT - 1);
		
	}
	
}

/* Caso seja iniciada uma eleicao, o servidor participante vai enviar para o servidor controle uma mensagem querendo ingressar na eleicao. Essa mensagem
se constitui de uma struct ELECTION_DATA contendo o id do server e um inteiro dizendo q esta participando.
	Caso o servidor participante detecte a mensagem ELECTION_TOKEN do servidor controle, ele deve chamar esta funcao
*/

void election_join(int this_server_id, int socket_control_server){
	ELECTION_DATA new_form;
	
	new_form.server_id = this_server_id;
	printf("Servidor %d entrando na eleicao \n", this_server_id);
	send_message(new_form, socket_control_server); // Substituir pela funcao implementada, com os argumentos adequados, new_form é a mensagem, e o socket o destino
	//Lembrando q new_form eh uma struct mas que só enviaremos o id do server participante para o controle
}

//Caso o servidor participante receba a mensagem ELECTION_RESULT do controle, ele se torna o lider
void is_new_leader(int* this_server.leader){
	this_server.leader = 1;
}
	
/*============================================================================================================================================================
	A partir daqui serao as funcoes que devem ser implementadas no servidor de controle
	Assumimos que todos os servidores estão cadastrados numa lista do tipo SERVER_DATA que constitui uma lista encadeada, caso optem por alterar para um array 
devemos alterar os loops
==============================================================================================================================================================*/ 

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
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
