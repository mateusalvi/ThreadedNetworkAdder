############################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
############################################################

RunServer.o: RunClient.o RunServer.c discovery.h processing.h constants.h  server_prot.h
	gcc RunServer.c -o RunServer discovery.c processing.c server_prot.c -lpthread

RunClient.o: RunClient.c discovery.h processing.h constants.h client.h 
	gcc RunClient.c -o RunClient discovery.c processing.c client.c -lpthread

#NetworkAdder.o: NetworkAdder.c discovery.h processing.h constants.h client.h server_prot.h
#	gcc NetworkAdder.c -o NetworkAdder  discovery.c processing.c client.c server_prot.c -lpthread

clean:
	rm *.o RunClient RunServer