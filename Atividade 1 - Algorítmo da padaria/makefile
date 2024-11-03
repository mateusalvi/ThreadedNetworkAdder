#INF01151 - Sistemas Operacionais II N 
#Mateus Luiz Salvi - 

program.o: program.c liblamport.so
	gcc -L . -o program program.c -llamport -lpthread
liblamport.so: lamport.c lamport.h
	gcc -c -shared -fPIC -o liblamport.so lamport.c -lpthread

clean: 
	rm *.o program lamport.so