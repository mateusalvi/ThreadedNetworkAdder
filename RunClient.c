/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "client.h"

int main(int argc, char *argv[])
{
    if(argv[1])
        RunClient(atoi(argv[1]));
    return 0;
}