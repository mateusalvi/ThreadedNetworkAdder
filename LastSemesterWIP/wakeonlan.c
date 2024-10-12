/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "server.h"
#include "client.h"
#include "constants.h"

int main(int argc, char *argv[])
{
    bool isManager = false;

    if ((argv[1] != NULL) && (strcmp(argv[1], "$manager"))) //Level 1+ optimizations must be enabled in compiler (boolean short-circuit)
        isManager = true;

    if(isManager)
    {
        RunServer();
    }
    else
    {
        RunClient();
    }

    return 0;
}