/**
 * @file server_init.c
 * @brief Implementazione della funzione server_init
 * @author Gionatha Sturba 531274
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 */

#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/select.h>
#include<sys/time.h>
#include"utils.h"
#include"server.h"
#include"config.h"


server_t* init_server(char *sockname,size_t messageSize,int max_connection)
{
    int err,curr_error;
    server_t *server;

    //controllo parametri
    if(sockname == NULL || messageSize <= 0 || max_connection <= 0 )
    {
        errno = EINVAL;
        return NULL;
    }

    //alloco istanza del server e controllo esito
    server = (server_t*)malloc(sizeof(server_t));

    error_handler_1(server,NULL,NULL);

    //inizializzazione indirizzo server

    //path del server
    err = snprintf(server->sa.sun_path,UNIX_PATH_MAX,"%s",sockname);

    //controllo errore snprintf
    if(err < 0)
    {
        //@see man snprintf return value
        errno = EIO;
        free(server);
        return NULL;
    }

    //setto il tipo di connessione
    server->sa.sun_family = AF_UNIX;

    //inzializzo il socket
    server->fd = socket(AF_UNIX,SOCK_STREAM,0);

    //controllo errore socket
    if(server->fd == -1)
    {
        free(server);
        return NULL;
    }

    //assegno indirizzo al socket
    err = bind(server->fd,(struct sockaddr *)&server->sa,sizeof(server->sa));

    //controllo errore bind
    if(err == -1)
    {
        close(server->fd);
        free(server);
        return NULL;
    }

    //metto in ascolto
    err = listen(server->fd,SOMAXCONN);

    //controllo errore listen
    if(err == -1)
    {
        //se la unlink fallisce ritorno l'errore della listen
        curr_error = errno;

        //rimuovo indirizzo fisico
        if(unlink(server->sa.sun_path) == -1)
        {
            errno = curr_error;
        }

        close(server->fd);
        free(server);
        return NULL;
    }

    //setto max connection e la size del messaggio che riceve dal client
    server->max_connection = max_connection;
    server->messageSize = messageSize;

    //inizializzazione andata a buon fine
    return server;
}
