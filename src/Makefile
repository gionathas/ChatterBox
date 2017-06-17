CC = gcc -std=c99
CFLAGS = -g -Wall
INCLUDES = ../include
LIBS_PATH = ../libs
LIBS = -lthreadpool -lqueue -lserver -lpthread

#objects
server_obj = server_start.o server_init.o
threadpool_obj = threadpool.o
queue_obj = queue.o

#target
server:
	make server_init.o
	make server_start.o
	make libserver.a
threadpool:
	make threadpool.o
	make libthreadpool.a
queue:
	make queue.o
	make libqueue.a

#generazione eseguibili
server_start.o:server_start.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

server_init.o:server_init.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

threadpool.o:threadpool.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

queue.o:queue.c
	$(CC) $(CFLAGS) -I $(INCLUDES) -c $<

#generazione librerie
libserver.a:$(server_obj)
	ar rvs $@ $^
	mv $@ ../libs

libqueue.a:$(queue_obj)
	ar rvs $@ $^
	mv $@ ../libs

libthreadpool.a:$(threadpool_obj)
	ar rvs $@ $^
	mv $@ ../libs
