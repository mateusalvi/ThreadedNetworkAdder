#include <stdio.h>
#include <stdin.h>
#include <string.h>
#include <math.h>





/*========================================================================================
  Funcao de soma a partir de uma string:
input: string de valores formatados em char, tamanho da string de entrada, ponteiro para resultado,
ponteiro para string saida (valor que deve ser impresso na tela do client formatado em string
output: void (na verdade esta nos proprios ponteiros)
DEVE SER ENCAIXADA NO LUGAR CERTO!!!! (eu nao me lembro onde era)
========================================================================================*/

void adder_implementation(char* string_in, int string_size, int* resultado, char* answer_to_client){
	int i, aux = (*resultado);
	
	// Esta soma ja devolve o resultado porem sem formatacao em string
	(*resultado) += atoi(string_in);

// Formatacao em string do valor somado
	for(i = 0; i < string_size; i++){
		answer_to_client[i] = floor((*resultado)/(10^(string_size - i)));
		aux = aux % (10 ^ (string_size - i));
	}

  // Forcar o '/0'
	i++;
	answer_to_client[i] = 0;
}
