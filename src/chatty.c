/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 * @author Gionatha Sturba 531274
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<assert.h>
#include<string.h>
#include<signal.h>
#include<pthread.h>
#include"connections.h"
#include"server.h"
#include"message.h"
#include"macro_error.h"
#include"chatty_task.h"
#include"stats.h"

/*
 * Struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

/* Funzioni di utilita' */

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

/* Ridenominazione delle funzioni vitali del server,per l'interfaccia di server.h */
static int server_read_message(int fd,void *mess)
{
    return readMsg((long)fd,(message_t*)mess);
}

static int server_signalusr1_handler(void* file)
{
    return printStats((FILE*)file);
}

static int server_client_overflow(int fd,void *arg)
{
    return chatty_clients_overflow(fd);
}


int main(int argc, char *argv[])
{
    server_t *server; //istanza del server
    server_config_t config; //variabile per salvare la cnfigurazione attuale del server
    server_function_t funs; //funzioni utili per il server
    int rc;

    //TODO parse argomenti

    //TODO Implementare file parser per la configurazione server

    //inizializzo e configuro il server
    server = init_server("tmp/server",sizeof(message_t),2);

    //controllo esito di init server
    err_null_ex(server,"MAIN: on init server");

    //inizializzo funzioni per il server

    //funzione per lettura messaggio
    funs.read_message_fun = server_read_message;
    //funzione per gestire troppi client,quest'ultima viene utilizzata dal listener
    funs.client_out_of_bound = server_client_overflow;
    //argomennti per la precedente funzione
    funs.arg_cob = NULL;
    //funzione per gestire l'arrivo del segnale USR1
    funs.signal_usr_handler = server_signalusr1_handler;
    //argomenti per la funzione precedente
    funs.arg_suh = NULL;

    //faccio partire il server,da ora in poi posso terminarlo solo con un segnale
    rc = start_server(server,2,funs);

    //controllo esito terminazione server.
    if(rc == -1 || rc == 1)
    {
        //caso di errore non riguardante il fallimento di uno o piu thread
        if(rc == -1)
        {
            perror("Server Fail");
        }

        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
