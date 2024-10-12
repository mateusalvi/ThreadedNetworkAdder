############################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/1) #
#                    Mateus Luiz Salvi                     #
##########################################################*/

wakeonlan.o: wakeonlan.c monitoring.h discovery.h management.h interface.h constants.h client.h server.h
	gcc wakeonlan.c -o wakeonlan monitoring.c discovery.c management.c interface.c client.c server.c -lpthread

clean:
	rm *.o wakeonlan