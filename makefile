############################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#       Adilson Enio Pierog - Andres Grendene Pacheco      #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
############################################################

CC=gcc
CFLAGS=-Wall -pthread
LDFLAGS=-lpthread

all: RunClient RunServer

RunClient: RunClient.o discovery.o processing.o client.o
	$(CC) RunClient.o discovery.o processing.o client.o -o RunClient $(LDFLAGS)

RunServer: RunServer.o server_prot.o discovery.o processing.o
	$(CC) RunServer.o server_prot.o discovery.o processing.o -o RunServer $(LDFLAGS)

RunClient.o: RunClient.c client.h
	$(CC) $(CFLAGS) -c RunClient.c

RunServer.o: RunServer.c server_prot.h
	$(CC) $(CFLAGS) -c RunServer.c

server_prot.o: server_prot.c server_prot.h discovery.h
	$(CC) $(CFLAGS) -c server_prot.c

discovery.o: discovery.c discovery.h
	$(CC) $(CFLAGS) -c discovery.c

processing.o: processing.c processing.h
	$(CC) $(CFLAGS) -c processing.c

client.o: client.c client.h discovery.h processing.h
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o RunClient RunServer

.PHONY: all clean


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
	docker run --rm -it --name $(SERVER_CONTAINER_NAME) $(SERVER_IMAGE_NAME) $(ARGS)

run_client_rands2: docker_client
	docker run --rm -i --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS) < rand2

run_client_rands3: docker_client
	docker run --rm -i --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS) < rand3

run_client_rands4: docker_client
	docker run --rm -i --name $(CLIENT_CONTAINER_NAME) $(CLIENT_IMAGE_NAME) $(ARGS) < rand4