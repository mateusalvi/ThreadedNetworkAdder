############################################################
# INF01151 - Sistemas Operacionais II N - Turma A (2024/2) #
#     Luís Filipe Martini Gastmann – Mateus Luiz Salvi     #
############################################################

CC=gcc
CFLAGS=-Wall -pthread
LDFLAGS=-lpthread
DEPS = server_prot.h discovery.h replication.h client.h
OBJ_SERVER = server_main.o server_prot.o discovery.o replication.o
OBJ_CLIENT = client_main.o client.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: RunServer RunClient

RunServer: $(OBJ_SERVER)
	$(CC) -o $@ $^ $(CFLAGS)

RunClient: $(OBJ_CLIENT)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o RunServer RunClient

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