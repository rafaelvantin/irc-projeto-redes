server: server.cpp
	g++ -g -o bin/server server.cpp -lpthread

client: client.cpp
	g++ -g -o bin/client client.cpp -lpthread

clean:
	rm -rf bin/*

all: server client