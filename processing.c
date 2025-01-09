/*========================================================================================
  Funcao de soma a partir de uma string:
input: string de valores formatados em char, tamanho da string de entrada, ponteiro para resultado,
ponteiro para string saida (valor que deve ser impresso na tela do client formatado em string
output: void (na verdade esta nos proprios ponteiros)
DEVE SER ENCAIXADA NO LUGAR CERTO!!!! (eu nao me lembro onde era)
========================================================================================*/

#include "processing.h"

void adder_implementation(int requestValue, int string_size, int *resultado, char *answer_to_client)
{
	printf("AAAAAAAAAAAAAAAAAAAAAAAAAAA");
	int i, aux = (*resultado);
	// Esta soma ja devolve o resultado porem sem formatacao em string
	(*resultado) += requestValue;
	printf("BBBBBBBBBBBBBBBBBBBBBBBBBBB");
	// Formatacao em string do valor somado
	for (i = 0; i < string_size - 1; i++)
	{
		printf("AAAAAAAAAAAAAAAAAAAAAAAAAAA");
		answer_to_client[i] = floor((*resultado) / (10 ^ (string_size - i)));
		printf("BBBBBBBBBBBBBBBBBBBBBBBBBBB");
		aux = aux % (10 ^ (string_size - i));
	}

	// Forcar o '/0'
	i++;
	answer_to_client[i] = 0;
	
}
