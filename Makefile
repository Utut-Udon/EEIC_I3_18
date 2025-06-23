CC = gcc

SERVER_SRC = server.c
CLIENT_SRC = client.c

SERVER_TARGET = server
CLIENT_TARGET = client

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_SRC)
	$(CC) -o $(SERVER_TARGET) $(SERVER_SRC) -pthread

$(CLIENT_TARGET): $(CLIENT_SRC)
	$(CC) -o $(CLIENT_TARGET) $(CLIENT_SRC) -pthread

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

re: clean all
