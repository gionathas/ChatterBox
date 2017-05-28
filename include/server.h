#include<pthread.h>
#include<sys/socket.h>
#include<sys/un.h>
#include"threadpool.h"

#define SERVER_ERR_HANDLER(err,val,ret)         \
    do{if( (err) == (val)){return (ret);} }while(0)

typedef struct server{
    threadpool_t *threadpool;
    int fd;
    struct sockaddr_un sa; //indirizzo server
}server_t;

//struttura delle funzioni che vengono passate al server
typedef struct {
    void (*function)(void *,int,void*); // primo argomento buffer,2 fd_client,3 arg
    void *arg;
}client_manager_fun_t;

//struttura per gestire il caso in cui ci sono troppi client.Deve eseguirla il server e non un thread del pool
typedef struct{
    void (*function)(int,void *); //1 fd client,2 argomento arg
    void *arg;
}too_much_client_handler_t;

server_t* init_server(char *sockname,int num_pool_thread);

void start_server(server_t *server,size_t messageSize,int max_connection,client_manager_fun_t cl_manager,too_much_client_handler_t cl_outbound);
