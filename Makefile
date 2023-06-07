default:
	echo "use: make build; make client; make server; make clean"

build/relay_client.o:
	gcc -g -fsanitize=address -fsanitize=undefined -c -o build/relay_client.o lib/relay_client.c -I./lib

build/relay_server.o:
	gcc -g -fsanitize=address -fsanitize=undefined -c -o build/relay_server.o lib/relay_server.c -I./lib

build/client.o:
	gcc -g -fsanitize=address -fsanitize=undefined -c -o build/client.o bin/client.c -I./lib

build/server.o:
	gcc -g -fsanitize=address -fsanitize=undefined -c -o build/server.o bin/server.c -I./lib

client: build/client.o build/relay_client.o
	gcc -g -fsanitize=address -fsanitize=undefined -o client build/client.o build/relay_client.o

server: build/server.o build/relay_server.o
	gcc -g -fsanitize=address -fsanitize=undefined -o server build/server.o build/relay_server.o

build:
	mkdir build

clean:
	rm client server build/client.o build/server.o build/relay_client.o build/relay_server.o