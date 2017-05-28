CC	= gcc -std=c99   						# quale compilatore usare per compilare il codice
CFLAGS	= -g -Wall       					# flags passati al compilatore per debugging e warning etc..
OPTFLAGS= -O3           					 # flags passati al compilatore per ottimizzazione del codice
INCLUDES= -I ../include        				 # path dove si trovano gli include (non di sistema)
LDFLAGS = -L ../libs           				# path dove so trovano le librerie
LIBS    = -lpthread -lserver -lthreadpool  -lqueue # vanno messe qui le librerie da linkare

queue_obj = queue.o
server_obj = server.o
threadpool_obj = threadpool.o

#comando principale per il test attuale
test:server client

lib:
	make libqueue.a
	make libthreadpool.a
	make libserver.a

#eseguibili test
server: server_test.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)

server_test.o:server_test.c
	$(CC) $(CFLAGS) $(INCLUDES)  -c $<

client: client_test.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)

client_test.o:client_test.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

#oggetti librerie
queue.o:queue.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
	make libqueue.a

threadpool.o:threadpool.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
	make libthreadpool.a

server.o:server.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
	make libserver.a

#creazioni librerie
libqueue.a: $(queue_obj)
	ar rvs $@ $^
	mv $@ ../libs

libthreadpool.a: $(threadpool_obj)
	ar rvs $@ $^
	mv $@ ../libs

libserver.a: $(server_obj)
	ar rvs $@ $^
	mv $@ ../libs
