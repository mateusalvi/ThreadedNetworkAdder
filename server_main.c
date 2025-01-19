#include <stdio.h>
#include <stdlib.h>
#include "server_prot.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    init_server(atoi(argv[1]));
    return 0;
}
