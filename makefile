############################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

NetworkAdder.o: NetworkAdder.c discovery.h processing.h interface.h constants.h client.h server.h
	gcc NetworkAdder.c -o NetworkAdder  discovery.c processing.c interface.c client.c server.c -lpthread

clean:
	rm *.o NetworkAdder