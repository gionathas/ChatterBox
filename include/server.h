/**
 * @file server.h
 * @brief Header file del server
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef SERVER_H
#define SERVER_H_

#include<pthread.h>
#include<sys/socket.h>
#include<sys/un.h>
#include"threadpool.h"

#define SERVER_ERR_HANDLER(err,val,ret)         \
    do{if( (err) == (val)){return (ret);} }while(0)

/**
 * @struct server_t
 * @brief struttura del server
 *
 * @var threadpool threadpool del server
 * @var sa indirizzo del server
 * @var max_connection numero massimo di connessioni accettate dal server
 * @var messageSize size dei messaggi che riceve dai client
 * @var listener_thread id del thread lsitener
 */
typedef struct server{
    threadpool_t *threadpool;
    int fd;
    struct sockaddr_un sa; //indirizzo server
    int max_connection;
    size_t messageSize;
    pthread_t listener_thread;
}server_t;

/**
 * @struct server_function_t
 * @brief Struttura delle funzioni da passare al server
 *
 * @var client_manager_fun funzione per gestire il client
 * @var arg_cmf argomenti per la funzione client_manager_fun
 * @var read_message_fun funzione per leggere il messaggio da un client
 * @var signal_usr_handler funzione per gestire il segnale sigusr1
 * @var arg_suh argomenti per la funzione signal_usr_handler
 * @var client_out_of_bound funzione per gestire il caso in cui ci sono troppi client
 * @var arg_cob argomenti per la funzione client_out_of_bound
 */
typedef struct{
    void (*client_manager_fun)(void *,int,void*); // 1 buffer,2 fd_client,3 arg
    void *arg_cmf;
    int (*read_message_fun)(int,void*); //1 fd client, 2 buffer
    void (*signal_usr_handler)(void*);//solo argomenti
    void *arg_suh;
    void (*client_out_of_bound)(int,void*);//1 fd, 2 argomenti
    void *arg_cob;
}server_function_t;

/**
 * @function init_server
 * @brief inizializza e configuro un nuovo server di tipo AF_UNIX
 *
 * @param sockname path del socket dove risiede il server
 * @param message size del messaggio che puo' ricevere il server
 * @param max_connection numero massimo di client che possono essere connessine
 *
 * @return on success puntatore al server,altimenti NULL e setta errno.
 */
server_t* init_server(char *sockname,size_t messageSize,int max_connection);

/**
 * @function start_server
 * @brief Fa partire il server
 *
 * @param srv istanza del server da far partire
 * @param num_pool_thread numero di thread del pool da far partire
 * @param funs struttura delle funzioni utilizzate dal server
 *
 * @return Ritora 0 quando il server e' terminato con un segnale di terminazione,
 *        altrimenti ritorna -1 e setta errno.
 *
 * @note Il server viene terminato con un segnale(SIGTERM,SIGQUIT,SIGINT).
 */
int start_server(server_t *srv,int num_pool_thread,server_function_t funs);

#endif //SERVER_H_
