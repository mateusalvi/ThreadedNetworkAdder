############################################################
#                                                          #
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#           Mateus Luiz Salvi - Bianca Pelegrini           #
#                                                          #
############################################################

program.o: program.c libbakery.so
	gcc -L . -o program program.c -lbakery
libbakery.so: libbakery.c libbakery.h
	gcc -c -shared -fPIC -o libbakery.so libbakery.c

clean: 
	rm *.o program libbakery.so