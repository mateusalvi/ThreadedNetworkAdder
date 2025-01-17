/*========================================================================================
  Funcao de soma a partir de uma string:
input: string de valores formatados em char, tamanho da string de entrada, ponteiro para resultado,
ponteiro para string saida (valor que deve ser impresso na tela do client formatado em string
output: void (na verdade esta nos proprios ponteiros)
DEVE SER ENCAIXADA NO LUGAR CERTO!!!! (eu nao me lembro onde era)
========================================================================================*/

#include "processing.h"

void adder_implementation(int requestValue, int string_size, int *resultado, char *answer_to_client) {
    // Atualiza o resultado acumulado
    *resultado += requestValue;

    // Formata o resultado em string
    sprintf(answer_to_client, "%d", *resultado);
}