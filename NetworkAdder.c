/*##########################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

#include "constants.h"
#include "server.h"
#include "client.h"

int main(int argc, char *argv[])
{
    bool isManager = false;

    if ((argv[1] != NULL) && (strcmp(argv[1], "$manager")))
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