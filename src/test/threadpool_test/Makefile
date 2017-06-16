test: threadpool_test.o
	gcc -g -Wall -std=c99 -I ../../../include threadpool_test.o -o test -L ../../../libs -lthreadpool -lqueue -lpthread

threadpool_test.o: threadpool_test.c
	gcc -g -Wall -std=c99 -I ../../../include -c threadpool_test.c
