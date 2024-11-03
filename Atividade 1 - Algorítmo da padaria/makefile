# INF01151 - Sistemas Operacionais II N
# Mateus Luiz Salvi – 00229787
# Adilson Enio Pierog – 00158803
# Andres Grendene Pacheco - 00264397
# Luís Filipe Martini Gastmann - 00276150

program.o: program.c liblamport.so
	gcc -L . -o program program.c -llamport -lpthread
liblamport.so: lamport.c lamport.h
	gcc -c -shared -fPIC -o liblamport.so lamport.c -lpthread

clean: 
	rm *.o program lamport.so