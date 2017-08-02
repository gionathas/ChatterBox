
/**
 * @file server_start.c
 * @brief Implementazione della funzione server_start
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */

 /*macro per warning di funzioni */
#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200112L
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<signal.h>
#include<sys/select.h>
#include<sys/time.h>
#include"utils.h"
#include"server.h"
#include"queue.h"

//@see server_thread_error in server.h
int server_thread_error;

int start_server(server_t *server,int num_pool_thread,server_function_t funs)
{
    int err = 0,curr_error;
    sigset_t signal_mask,old_mask;

    //controllo parametri di inizializzazione del server
    if(server == NULL || funs.client_manager_fun == NULL || funs.read_message_fun == NULL || funs.client_out_of_bound == NULL || funs.disconnect_client == NULL)
    {
        errno = EINVAL;
        curr_error = errno;
        goto st_error;
    }

    server_thread_error = 0;

    //blocco tutti i segnali per i thread del pool,e per questo thread che fa la join
    sigfillset(&signal_mask);
    err = pthread_sigmask(SIG_SETMASK,&signal_mask,&old_mask);

    if(err)
    {
        errno = err;
        curr_error = errno;
        goto st_error;
    }

    //faccio partire il threadpool
    server->threadpool = threadpool_create(num_pool_thread);

    //errore creazione threadpool
    if(server->threadpool == NULL)
    {
        curr_error = errno;
        goto st_error;
    }

    //Inizializzo le funzioni globali
    client_manager_fun = funs.client_manager_fun;
    arg_cmf = funs.arg_cmf;

    signal_usr_handler = funs.signal_usr_handler;
    arg_suh = funs.arg_suh;

    client_out_of_bound = funs.client_out_of_bound;
    arg_cob = funs.arg_cob;

    disconnect_client = funs.disconnect_client;
    arg_dc = funs.arg_dc;

    read_message_fun = funs.read_message_fun;


    //avvio listener
    err = pthread_create(&server->listener_thread,NULL,listener,(void*)server);

    //errore create
    if(err)
    {
        errno = err;
        curr_error = errno;
        goto st_error2;
    }

    int status;

    //metto questo thread in attesa con una join sul thread del listener
    err = pthread_join(server->listener_thread,(void*)&status);

    //errore join
    if(err)
    {
        errno = err;
        curr_error = errno;
        goto st_error2;
    }

    //errore server
    if(status == EXIT_FAILURE)
    {
        //se e' fallito un thread,ritorno il codice dell'errore con cui e' fallito
        if(server_thread_error > 0)
            return server_thread_error;
        //altrimenti c'e' stato un errore interno del server. Ritorno solo -1,errno gia settato
        else
            return -1;
    }
    //tutto ok
    else
        return 0;

//error_handling
st_error2:
    threadpool_destroy(&server->threadpool);
    goto st_error;
st_error:

    //rimuovo indirizzo fisico
    unlink(server->sa.sun_path);

    close(server->fd);
    free(server);

    //risetto errno per sicurezza,cioe' che una funzione appena chiamata non fallisca e mi cambi errno
    errno = curr_error;
    return -1;
}
