CC = gcc -std=c99
CFLAGS = -g -Wall -pedantic -DMAKE_VALGRIND_HAPPY
INCLUDES = ../include
LIBS_PATH = ../libs
LIBS = -lserver -lpthread -lchatty -lm -lqueue -lthreadpool

#objects
server_obj = server_start.o server_init.o listener.o
threadpool_obj = threadpool.o
queue_obj = queue.o
chatty_obj = chatty.o connections.o chatty_parser.o messaggi_utenti.o gestione_utenti.o chatty_clients.o

#target
_chatty:
	make connections.o
	make chatty_parser.o
	make messaggi_utenti.o
	make gestione_utenti.o
	make chatty_clients.o
	make libchatty.a
server:
	make server_init.o
	make server_start.o
	make listener.o
	make libserver.a
threadpool:
	make threadpool.o
	make libthreadpool.a
queue:
	make queue.o
	make libqueue.a

#processi principali
chatty:chatty.o
	$(CC) $(CFLAGS) -I $(INCLUDES) $< -o $@ -L $(LIBS_PATH) $(LIBS)

client:client.o
	$(CC) $(CFLAGS) -I $(INCLUDES) $< -o $@ -L $(LIBS_PATH) $(LIBS)

#generazione eseguibili
chatty.o: chatty.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

client.o:client.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

chatty_parser.o:chatty_parser.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

chatty_clients.o:chatty_clients.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

connections.o:connections.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

messaggi_utenti.o:messaggi_utenti.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

gestione_utenti.o:gestione_utenti.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

listener.o:listener.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

server_start.o:server_start.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

server_init.o:server_init.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

threadpool.o:threadpool.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

queue.o:queue.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

#generazione librerie
libchatty.a: $(chatty_obj)
	ar rvs $@ $^
	mv $@ ../libs

libserver.a:$(server_obj)
	ar rvs $@ $^
	mv $@ ../libs

libqueue.a:$(queue_obj)
	ar rvs $@ $^
	mv $@ ../libs

libthreadpool.a:$(threadpool_obj)
	ar rvs $@ $^
	mv $@ ../libs
