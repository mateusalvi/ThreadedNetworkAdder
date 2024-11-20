/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"
#include "server_prot.h"
//#include "server.h"
#include "client.h"

int main(int argc, char *argv[])
{
    int input;
    while(1)
    {
        printf("1 for client, 2 for server\n");
        scanf("%d", &input);
        switch (input)
        {
        case 1:
            RunClient(atoi(argv[1]));
            break;
        case 2:
            ServerMain(argv[1]);
            break;
        
        default:
            break;
        }
    }
    return 0;
}