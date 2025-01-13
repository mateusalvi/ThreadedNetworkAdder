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


############################################################################################################################
#########					DOCKER
############################################################################################################################
# Define variables
CLIENT_IMAGE_NAME := my_client_image
SERVER_IMAGE_NAME := my_server_image
CLIENT_CONTAINER_NAME := client_container_$(shell date +%s)
SERVER_CONTAINER_NAME := server_container_$(shell date +%s)
# INSTANCE_COUNT := $(or $(INSTANCES), 1)

# Build the Docker image
docker_build: docker_client docker_server

docker_client:
	docker build -t $(CLIENT_IMAGE_NAME) -f Dockerfile.client .
docker_server:
	docker build -t $(SERVER_IMAGE_NAME) -f Dockerfile.server .


# Run the Docker container with a unique name
run_client: docker_client
	docker run --rm -it --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS)
run_server: docker_server
	docker run -it --name $(SERVER_CONTAINER_NAME) $(SERVER_IMAGE_NAME) $(ARGS)

run_client_rands2: docker_client
	docker run --rm -i --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS) < rand2

run_client_rands3: docker_client
	docker run --rm -i --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS) < rand3

run_client_rands4: docker_client
	docker run --rm -i --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS) < rand4