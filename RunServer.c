/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
##########################################################*/

#include "server_prot.h"

int main(int argc, char *argv[])
{
    if(argv[1])
        ServerMain(argv[1]);
    return 0;
}