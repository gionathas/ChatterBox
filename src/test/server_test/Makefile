all:server client

server: server_test.o
	gcc -g -Wall -std=c99 -I ../../../include server_test.o -o server -L ../../../libs -lserver -lqueue -lpthread -lthreadpool

server_test.o:server_test.c
	gcc -g -Wall -I ../../../include -c server_test.c

client:client_test.o
	gcc -g -Wall -std=c99 -I ../../../include client_test.o -o client
client_test.o:client_test.c
	gcc -g -Wall -I ../../../include -c client_test.c
